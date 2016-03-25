#ifndef DNG_OPCODES_H
#define DNG_OPCODES_H
#include <vector>
#include "TiffIFD.h"
#include "RawImage.h"
/* 
RawSpeed - RAW file decoder.

Copyright (C) 2012 Klaus Post

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

class DngOpcode
{
public:
  DngOpcode(void) {host = getHostEndianness();};
  virtual ~DngOpcode(void) {};

  /* Will be called exactly once, when input changes */
  /* Can be used for preparing pre-calculated values, etc */
  virtual RawImage& createOutput(RawImage &in) {return in;}
  /* Will be called for actual processing */
  /* If multiThreaded is TRUE, it will be called several times, */
  /* otherwise only once */
  /* Properties of out will not have changed from createOutput */
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY) = 0;
  iRectangle2D mAoi;
  int mFlags;
  enum Flags
  {
    MultiThreaded = 1,
    PureLookup = 2
  };

  
protected:
  Endianness host;
  int32 getLong(const uchar8 *ptr) {
    if (host == big)
      return *(int32*)ptr;
    return (int32)ptr[0] << 24 | (int32)ptr[1] << 16 | (int32)ptr[2] << 8 | (int32)ptr[3];
  }
  uint32 getULong(const uchar8 *ptr) {
    if (host == big)
      return *(uint32*)ptr;
    return (uint32)ptr[0] << 24 | (uint32)ptr[1] << 16 | (uint32)ptr[2] << 8 | (uint32)ptr[3];
  }
  double getDouble(const uchar8 *ptr) {
    if (host == big)
      return *(double*)ptr;
    double ret;
    uchar8 *tmp = (uchar8*)&ret;
    for (int i = 0; i < 8; i++)
     tmp[i] = ptr[7-i];
    return ret;
  }
  float getFloat(const uchar8 *ptr) {
    if (host == big)
      return *(float*)ptr;
    float ret;
    uchar8 *tmp = (uchar8*)&ret;
    for (int i = 0; i < 4; i++)
      tmp[i] = ptr[3-i];
    return ret;
  }
  ushort16 getUshort(const uchar8 *ptr) {
    if (host == big)
      return *(ushort16*)ptr;
    return (ushort16)ptr[0] << 8 | (ushort16)ptr[1];
  }

};


class DngOpcodes
{
public:
  DngOpcodes(TiffEntry *entry);
  virtual ~DngOpcodes(void);
  RawImage& applyOpCodes(RawImage &img);
private:
  vector<DngOpcode*> mOpcodes;
  Endianness host;
  uint32 getULong(const uchar8 *ptr) {
    if (host == big)
      return *(uint32*)ptr;
    return (uint32)ptr[0] << 24 | (uint32)ptr[1] << 16 | (uint32)ptr[2] << 8 | (uint32)ptr[3];
  }
};

class OpcodeFixBadPixelsConstant: public DngOpcode
{
public:
  OpcodeFixBadPixelsConstant(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeFixBadPixelsConstant(void) {};
  virtual RawImage& createOutput( RawImage &in );
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  int mValue;
};


class OpcodeFixBadPixelsList: public DngOpcode
{
public:
  OpcodeFixBadPixelsList(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeFixBadPixelsList(void) {};
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  vector<uint32> bad_pos;
};


class OpcodeTrimBounds: public DngOpcode
{
public:
  OpcodeTrimBounds(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeTrimBounds(void) {};
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mTop, mLeft, mBottom, mRight;
};


class OpcodeMapTable: public DngOpcode
{
public:
  OpcodeMapTable(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeMapTable(void) {};
  virtual RawImage& createOutput(RawImage &in);
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mFirstPlane, mPlanes, mRowPitch, mColPitch;
  ushort16 mLookup[65536];
};

class OpcodeMapPolynomial: public DngOpcode
{
public:
  OpcodeMapPolynomial(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeMapPolynomial(void) {};
  virtual RawImage& createOutput(RawImage &in);
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mFirstPlane, mPlanes, mRowPitch, mColPitch, mDegree;
  double mCoefficient[9];
  ushort16 mLookup[65536];
};

class OpcodeDeltaPerRow: public DngOpcode
{
public:
  OpcodeDeltaPerRow(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeDeltaPerRow(void) {};
  virtual RawImage& createOutput(RawImage &in);
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mFirstPlane, mPlanes, mRowPitch, mColPitch, mCount;
  float* mDelta;
};

class OpcodeDeltaPerCol: public DngOpcode
{
public:
  OpcodeDeltaPerCol(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeDeltaPerCol(void);
  virtual RawImage& createOutput(RawImage &in);
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mFirstPlane, mPlanes, mRowPitch, mColPitch, mCount;
  float* mDelta;
  int* mDeltaX;
};

class OpcodeScalePerRow: public DngOpcode
{
public:
  OpcodeScalePerRow(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeScalePerRow(void) {};
  virtual RawImage& createOutput(RawImage &in);
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mFirstPlane, mPlanes, mRowPitch, mColPitch, mCount;
  float* mDelta;
};

class OpcodeScalePerCol: public DngOpcode
{
public:
  OpcodeScalePerCol(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used);
  virtual ~OpcodeScalePerCol(void);
  virtual RawImage& createOutput(RawImage &in);
  virtual void apply(RawImage &in, RawImage &out, uint32 startY, uint32 endY);
private:
  uint64 mFirstPlane, mPlanes, mRowPitch, mColPitch, mCount;
  float* mDelta;
  int* mDeltaX;
};

} // namespace RawSpeed 

#endif // DNG_OPCODES_H
