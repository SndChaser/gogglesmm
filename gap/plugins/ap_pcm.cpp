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
#include "ap_packet.h"
#include "ap_engine.h"
#include "ap_output_thread.h"
#include "ap_decoder_plugin.h"

namespace ap {

class PCMDecoder : public DecoderPlugin {
public:
  PCMDecoder(AudioEngine*);
  FXuchar codec() const override { return Codec::PCM; }
  FXbool init(ConfigureEvent*) override;
  DecoderStatus process(Packet*) override;
  virtual ~PCMDecoder();
  };

PCMDecoder::PCMDecoder(AudioEngine * e) : DecoderPlugin(e) {
  }

PCMDecoder::~PCMDecoder() {
  }

FXbool PCMDecoder::init(ConfigureEvent*event) {
  DecoderPlugin::init(event);
  return true;
  }


DecoderStatus PCMDecoder::process(Packet*in) {
  FXbool eos    = (in->flags&FLAG_EOS);
  FXint  stream = in->stream;

  /// Simply Forward to output
  engine->output->post(in);

  if (eos) {
    engine->output->post(new ControlEvent(End,stream));
    }
  return DecoderOk;
  }

DecoderPlugin * ap_pcm_decoder(AudioEngine * engine) {
  return new PCMDecoder(engine);
  }

}
