/*******************************************************************************
*                         Goggles Audio Player Library                         *
********************************************************************************
*           Copyright (C) 2010-2016 by Sander Jansen. All Rights Reserved      *
*                               ---                                            *
* This program is free software: you can redistribute it and/or modify         *
* it under the terms of the GNU General Public License as published by         *
* the Free Software Foundation, either version 3 of the License, or            *
* (at your option) any later version.                                          *
*                                                                              *
* This program is distributed in the hope that it will be useful,              *
* but WITHOUT ANY WARRANTY; without even the implied warranty of               *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                *
* GNU General Public License for more details.                                 *
*                                                                              *
* You should have received a copy of the GNU General Public License            *
* along with this program.  If not, see http://www.gnu.org/licenses.           *
********************************************************************************/
#include "ap_defs.h"
#include "ap_event_private.h"
#include "ap_packet.h"
#include "ap_engine.h"
#include "ap_ogg_decoder.h"
#include "ap_decoder_thread.h"


#include <opus/opus_multistream.h>


namespace ap {


class OpusDecoderPlugin : public OggDecoder{
protected:
  OpusMSDecoder* opus;
  FXfloat      * pcm;
  FXfloat        gain;
  FXushort       stream_offset_start;
protected:
  FXbool init_decoder();
  FXbool find_stream_position();
  FXlong find_stream_length();
protected:
public:
  OpusDecoderPlugin(AudioEngine*);

  FXuchar codec() const override { return Codec::Opus; }
  FXbool init(ConfigureEvent*) override;
  DecoderStatus process(Packet*) override;
  FXbool flush(FXlong) override;


  virtual ~OpusDecoderPlugin();
  };


OpusDecoderPlugin::OpusDecoderPlugin(AudioEngine * e) : OggDecoder(e),opus(nullptr),pcm(nullptr),gain(0.0f),stream_offset_start(0) {
  }

OpusDecoderPlugin::~OpusDecoderPlugin(){
  freeElms(pcm);
  if (opus) {
    opus_multistream_decoder_destroy(opus);
    opus=nullptr;
    }
  }

#define MAX_FRAME_SIZE (960*6)

FXbool OpusDecoderPlugin::init(ConfigureEvent*event) {
  OggDecoder::init(event);
  af=event->af;

  if (opus) {
    opus_multistream_decoder_destroy(opus);
    opus=nullptr;
    gain=0.0f;
    }

  if (pcm)
    resizeElms(pcm,MAX_FRAME_SIZE*af.channels*2);
  else
    allocElms(pcm,MAX_FRAME_SIZE*af.channels*2);

  stream_offset_start = event->stream_offset_start;
  return true;
  }


FXbool OpusDecoderPlugin::flush(FXlong offset) {
  OggDecoder::flush(offset);
  //if (opus) opus_decoder_ctl(opus,OPUS_RESET_STATE);
  return true;
  }


FXbool OpusDecoderPlugin::find_stream_position() {
  const FXuchar * data_ptr = get_packet_offset();
  FXlong    nsamples = 0;
  GM_DEBUG_PRINT("[opus] find stream position\n");
  while(get_next_packet()) {
    nsamples += opus_packet_get_nb_samples((unsigned char*)op.packet,op.bytes,48000);
    if (op.granulepos>=0) {
      GM_DEBUG_PRINT("[opus] found stream position: %ld\n",op.granulepos-nsamples);
      stream_position=op.granulepos-nsamples;
      set_packet_offset(data_ptr);
      return true;
      }
    }
  set_packet_offset(data_ptr);
  return false;
  }

FXlong OpusDecoderPlugin::find_stream_length() {
  const FXuchar * data_ptr = get_packet_offset();
  FXlong    nlast = 0;
  GM_DEBUG_PRINT("[opus] find stream length\n");
  while(get_next_packet()) {
    nlast = op.granulepos;
    }
  set_packet_offset(data_ptr);
  return nlast - stream_offset_start;
  }



FXbool OpusDecoderPlugin::init_decoder() {
  FXASSERT(opus==nullptr);
  FXint error;

  if (get_next_packet() && op.bytes>=19) {

    // Extra check to make sure the reader gave us the header packet
    if (compare((const FXchar*)op.packet,"OpusHead",8))
      return false;

    if (op.packet[18]!=0) {

      // Validate stream map size
      if (af.channels!=op.bytes-21)
        return false;

      // Validate stream map
      const FXuchar nstreams=op.packet[19];
      const FXuchar ncoupled=op.packet[20];
      for (FXint i=0;i<af.channels;i++) {
        if (op.packet[21+i]>nstreams+ncoupled && op.packet[21+i]!=255)
          return false;
        }
      opus = opus_multistream_decoder_create(48000,af.channels,nstreams,ncoupled,op.packet+21,&error);
      }
    else {
      const FXuchar stream_map[2] = {0,1};
      opus = opus_multistream_decoder_create(48000,af.channels,1,af.channels>1,stream_map,&error);
      }

    if (error!=OPUS_OK)
      return false;


    // Apply any gain
#if FOX_BIGENDIAN == 0
    FXshort output_gain = op.packet[16] | op.packet[17]<<8;
#else
    FXshort output_gain = op.packet[16]<<8 | op.packet[17];
#endif

#ifdef OPUS_SET_GAIN
    error=opus_multistream_decoder_ctl(opus,OPUS_SET_GAIN(output_gain));
    if(error==OPUS_UNIMPLEMENTED)
      gain = pow(10,output_gain/(20.0*256));
    else
      gain = 0.0f;
#else
    gain = pow(10,output_gain/(20.0*256));
#endif
    return true;
    }
  return false;
  }


DecoderStatus OpusDecoderPlugin::process(Packet * packet) {
  const FXlong stream_begin  = FXMAX(stream_offset_start,stream_decode_offset);
  const FXlong stream_length = packet->stream_length;
  const FXbool eos           = packet->flags&FLAG_EOS;
  FXuint id                  = packet->stream;
  FXlong stream_end          = stream_length;

  OggDecoder::process(packet);

  if (opus==nullptr && !init_decoder())
    return DecoderError;

  if (stream_position==-1 && !find_stream_position())
    return DecoderOk;

  if (eos && stream_end==-1) {
    stream_end = find_stream_length();
    FXASSERT(stream_position-stream_offset_start<stream_end);
    }

  while(get_next_packet()) {
    FXint nsamples = opus_multistream_decode_float(opus,(unsigned char*)op.packet,op.bytes,pcm,MAX_FRAME_SIZE,0);

    const FXuchar * pcmi = (const FXuchar*)pcm;

    // apply output gain
    if (gain!=0.0f) {
      for (FXint i=0;i<nsamples*af.channels;i++) {
        pcm[i]*=gain;
        }
      }
    // Adjust for beginning of stream
    if (stream_position<stream_begin) {
      FXlong offset = FXMIN(nsamples,stream_begin - stream_position);
      GM_DEBUG_PRINT("[opus] stream offset start %ld. Skip %ld at %ld\n",stream_begin,offset,stream_position);
      nsamples-=offset;
      pcmi+=(offset*af.framesize());
      stream_position+=offset;
      }

    //GM_DEBUG_PRINT("[opus] decoded %d frames\n",nsamples);
    if (eos) {
      FXlong total = stream_position-stream_offset_start+nsamples;
      if (total>stream_end) {
        GM_DEBUG_PRINT("adjusting end trimming by %ld\n",(total-stream_end));
        nsamples -= (total-stream_end);
        }
      }

    while(nsamples>0) {
      /// Get new buffer
      if (out==nullptr) {
        out = engine->decoder->get_output_packet();
        if (out==nullptr) return DecoderInterrupted;
        out->stream_position=stream_position - stream_offset_start;
        out->stream_length=stream_length;
        out->af=af;
        }

      FXint nw = FXMIN(out->availableFrames(),nsamples);
      if (nw>0){
        //fxmessage("add %d / %d / %d / %d\n",nw,nsamples,out->availableFrames(),af.framesize());
        out->appendFrames(pcmi,nw);
        pcmi+=(nw*af.framesize());
        nsamples-=nw;
        stream_position+=nw;
        }

      if (out->availableFrames()==0) {
        //fxmessage("posting\n");
        engine->output->post(out);
        out=nullptr;
        }
      }
    }

  if (eos) {
    if (out && out->numFrames())  {
      engine->output->post(out);
      out=nullptr;
      }
    engine->output->post(new ControlEvent(End,id));
    }
  return DecoderOk;
  }


DecoderPlugin * ap_opus_decoder(AudioEngine * engine) {
  return new OpusDecoderPlugin(engine);
  }


}
