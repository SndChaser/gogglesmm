/*******************************************************************************
*                         Goggles Music Manager                                *
********************************************************************************
*           Copyright (C) 2006-2010 by Sander Jansen. All Rights Reserved      *
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
#ifndef GMCOVERTHUMBS_H
#define GMCOVERTHUMBS_H

class GMImageMap;
class GMCompressedImage;

class FXIntMap : public FXHash {
public:
  FXint insert(FXint name,FXint value) { return (FXint)(FXival)FXHash::insert((void*)(FXival)name,(void*)(FXival)value); }
  FXint replace(FXint name,FXint value) { return (FXint)(FXival)FXHash::replace((void*)(FXival)name,(void*)(FXival)value); }
  FXint remove(FXint name) {  return (FXint)(FXival)FXHash::remove((void*)(FXival)name); }
  FXint find(FXint name) const { return (FXint)(FXival)FXHash::find((void*)(FXival)name); }
  FXint key(FXuint pos) const { return (FXint)(FXival)table[pos].name; }
  FXint value(FXuint pos) const { return (FXint)(FXival)table[pos].data; }
  void adopt(FXIntMap &);
  void load(FXStream & store);
  void save(FXStream & store) const;
  };



class GMCoverCache : FXObject {
FXDECLARE(GMCoverCache)
protected:
  FXPtrListOf<GMCompressedImage> covers;
  FXPtrListOf<FXImage>           buffers;
  FXIntMap                       map;
  FXint                          basesize;
  FXbool                         initialized;
protected:
  FXImage * getCoverImage(FXint id);
  void adopt(GMCoverCache &);
  FXbool load();
  FXString getCacheFile() const;
public:
  enum {
    ID_COVER_LOADER = 1
    };
public:
  long onCmdCoversLoaded(FXObject*,FXSelector,void*ptr);
public:
  GMCoverCache(FXint size=128);

  void init(GMTrackDatabase*);

  void refresh(GMTrackDatabase*);

  void drawCover(FXint id,FXDC & dc,FXint x,FXint y);

  void markCover(FXint id);

  void reset();

  void clear();

  FXint getCoverSize() const { return basesize; }

  void insertCover(FXint id,GMCompressedImage*);

  void save() const;

  ~GMCoverCache();
  };

#endif
