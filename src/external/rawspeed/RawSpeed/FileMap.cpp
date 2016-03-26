#include "StdAfx.h"
#include "FileMap.h"
/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

    http://www.klauspost.com
*/

namespace RawSpeed {

FileMap::FileMap(uint32 _size) : size(_size) {
  if (!size)
    throw FileIOException("Filemap of 0 bytes not possible");
  data = (uchar8*)_aligned_malloc(size + FILEMAP_MARGIN, 16);
  if (!data) {
    throw FileIOException("Not enough memory to open file.");
  }
  mOwnAlloc = true;
}

FileMap::FileMap(uchar8* _data, uint32 _size): data(_data), size(_size) {
  mOwnAlloc = false;
}

FileMap::FileMap(FileMap *f, uint32 offset) {
  size = f->getSize()-offset;
  data = f->getDataWrt(offset, size+FILEMAP_MARGIN);
  mOwnAlloc = false;
}

FileMap::FileMap(FileMap *f, uint32 offset, uint32 size) {
  data = f->getDataWrt(offset, size+FILEMAP_MARGIN);
  mOwnAlloc = false;
}

FileMap::~FileMap(void) {
  if (data && mOwnAlloc) {
    _aligned_free(data);
  }
  data = 0;
  size = 0;
}

FileMap* FileMap::clone() {
  FileMap *new_map = new FileMap(size);
  memcpy(new_map->data, data, size);
  return new_map;
}

FileMap* FileMap::cloneRandomSize() {
  uint32 new_size = (rand() | (rand() << 15)) % size;
  FileMap *new_map = new FileMap(new_size);
  memcpy(new_map->data, data, new_size);
  return new_map;
}

void FileMap::corrupt(int errors) {
  for (int i = 0; i < errors; i++) {
    uint32 pos = (rand() | (rand() << 15)) % size;
    data[pos] = rand() & 0xff;
  }
}

bool FileMap::isValid(uint32 offset, uint32 count)
{
  uint64 totaloffset = (uint64)offset + (uint64)count - 1;
  return (isValid(offset) && totaloffset < size);
}

const uchar8* FileMap::getData( uint32 offset, uint32 count )
{
  if (count == 0)
    throw IOException("FileMap: Trying to get a zero sized buffer?!");

  uint64 totaloffset = (uint64)offset + (uint64)count - 1;
  uint64 totalsize = (uint64)size + FILEMAP_MARGIN;

  // Give out data up to FILEMAP_MARGIN more bytes than are really in the
  // file as that is useful for some of the BitPump code
  if (!isValid(offset) || totaloffset >= totalsize)
    throw IOException("FileMap: Attempting to read file out of bounds.");
  return &data[offset];
}

} // namespace RawSpeed
