//-*****************************************************************************
//
// Copyright (c) 2009-2011,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#ifndef ABCIMPORT_NODE_ITERATOR_HELPER_H_
#define ABCIMPORT_NODE_ITERATOR_HELPER_H_

#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MDataHandle.h>
#include <maya/MArrayDataHandle.h>

#include <vector>
#include <string>

#include <Alembic/Abc/IArrayProperty.h>
#include <Alembic/Abc/IScalarProperty.h>
#include <Alembic/Abc/IObject.h>

#include <Alembic/AbcGeom/ICamera.h>
#include <Alembic/AbcGeom/ICurves.h>
#include <Alembic/AbcGeom/INuPatch.h>
#include <Alembic/AbcGeom/IPoints.h>
#include <Alembic/AbcGeom/IPolyMesh.h>
#include <Alembic/AbcGeom/ISubD.h>
#include <Alembic/AbcGeom/IXform.h>

// mArray or mScalar will be valid, mObj will be valid for those situations
// where the property can't be validly read, unless the object stays in scope.
struct Prop
{
    Alembic::Abc::IArrayProperty mArray;
    Alembic::Abc::IScalarProperty mScalar;
};

bool addProp(Alembic::Abc::IArrayProperty & iProp, MObject & iParent);

void addProps(Alembic::Abc::ICompoundProperty & iParent, MObject & iObject);

void readProp(double iFrame, Alembic::Abc::IArrayProperty & iProp,
    MDataHandle & iHandle);

void getAnimatedProps(Alembic::Abc::ICompoundProperty & iParent,
    std::vector<Prop> & oPropList);

// This class is used for connecting to sampled transform operations and
// properties in order  to keep the list of names of sampled channels
// associated with a particular MObject (mObject)
class SampledPair
{
public:
    SampledPair(MObject & iObject, std::vector<std::string> & sN):
        mObject(iObject), sampleNameList(sN)
    {
    }

    MObject & getObject()
    {
        return mObject;
    }

    unsigned int sampledChannelSize()
    {
        return static_cast<unsigned int>(sampleNameList.size());
    }

    std::string getSampleElement(unsigned int index)
    {
        if (index < sampleNameList.size())
            return sampleNameList[index];
        else
            return std::string();
    }

private:
    MObject mObject;
    std::vector<std::string> sampleNameList;
};  // SampledPair

// A data class for cleaner code when copying a group of info between functions
class WriterData
{
public:
    WriterData();
    ~WriterData();
    WriterData(const WriterData & rhs);
    WriterData & operator=(const WriterData & rhs);
    void getFrameRange(double & oMin, double & oMax);

    std::vector<MObject>        mCameraObjList;
    std::vector<MObject>        mLocObjList;
    std::vector<MObject>        mNurbsObjList;
    std::vector<MObject>        mNurbsCurveObjList;
    std::vector<MObject>        mPointsObjList;
    std::vector<MObject>        mPolyMeshObjList;
    std::vector<MObject>        mSubDObjList;

    std::vector<Prop> mPropList;
    std::vector<Alembic::AbcGeom::ICamera>    mCameraList;
    std::vector<Alembic::AbcGeom::ICurves>    mCurvesList;
    std::vector<Alembic::AbcGeom::IXform>     mLocList;
    std::vector<Alembic::AbcGeom::INuPatch>   mNurbsList;
    std::vector<Alembic::AbcGeom::IPolyMesh>  mPolyMeshList;
    std::vector<Alembic::AbcGeom::IPoints>    mPointsList;
    std::vector<Alembic::AbcGeom::ISubD>      mSubDList;
    std::vector<Alembic::AbcGeom::IXform>     mXformList;

    // objects that aren't animated but have animated visibility need to be
    // kept alive so the visibility can be read
    std::vector<Alembic::AbcGeom::IObject>    mAnimVisStaticObjList;

    // number of curves for each animated curve group
    std::vector<std::size_t>    mNumCurves;
    std::vector<bool>           mIsComplexXform;

    // only needed when doing connections
    std::vector<SampledPair>    mXformOpList;
    std::vector<SampledPair>    mPropObjList;

};  // WriterData

class ArgData
{
public:
    ArgData(MString iFileName = "",
        bool    iDebugOn = false,
        MObject iReparentObj = MObject::kNullObj,
        bool    iConnect = false,
        MString iConnectRootNodes = MString("/"),
        bool    iCreateIfNotFound = false,
        bool    iRemoveIfNoUpdate = false);
    ArgData(const ArgData & rhs);
    ArgData & operator=(const ArgData & rhs);

    MString     mFileName;
    double      mSequenceStartTime;
    double      mSequenceEndTime;
    bool        mDebugOn;

    MObject     mReparentObj;

    // optional information for the "connect" flag
    bool        mConnect;
    MString     mConnectRootNodes;
    bool        mCreateIfNotFound;
    bool        mRemoveIfNoUpdate;

    WriterData  mData;
};  // ArgData

// traverse and create the corresponding maya hierarchy
MString createScene(ArgData & iArgData);

// Called in createScene only if there are sampled data in the scene.
// This function sets up the connections to AlembicNode
MString connectAttr(ArgData & iArgData);

#endif  // ABCIMPORT_NODE_ITERATOR_HELPER_H_
