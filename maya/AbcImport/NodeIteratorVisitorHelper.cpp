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

#include <maya/MDoubleArray.h>
#include <maya/MIntArray.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MUint64Array.h>
#include <maya/MStringArray.h>
#include <maya/MFnData.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObjectArray.h>
#include <maya/MDGModifier.h>
#include <maya/MSelectionList.h>
#include <maya/MFnLight.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnCamera.h>
#include <maya/MTime.h>

#include <Alembic/AbcCoreHDF5/ReadWrite.h>

#include "util.h"
#include "AlembicNode.h"
#include "CreateSceneHelper.h"
#include "NodeIteratorVisitorHelper.h"

void unsupportedWarning(Alembic::Abc::IArrayProperty & iProp)
{
    MString warn = "Unsupported attr, skipping: ";
    warn += iProp.getName().c_str();
    warn += " ";
    warn += PODName(iProp.getDataType().getPod());
    warn += "[";
    warn += iProp.getDataType().getExtent();
    warn += "]";

    printWarning(warn);
}

void addString(MObject & iParent, const std::string & iAttrName,
    const std::string & iValue)
{
    MFnStringData fnStringData;
    MString attrValue(iValue.c_str());
    MString attrName(iAttrName.c_str());
    MObject strAttrObject = fnStringData.create(attrValue);

    MFnTypedAttribute attr;
    MObject attrObj = attr.create(attrName, attrName, MFnData::kString,
        strAttrObject);
    MFnDependencyNode parentFn(iParent);
    parentFn.addAttribute(attrObj, MFnDependencyNode::kLocalDynamicAttr);
}

void addArbAttrAndScope(MObject & iParent, const std::string & iAttrName,
    const std::string & iScope, const std::string & iInterp,
    Alembic::Util::uint8_t iExtent)
{

    std::string attrStr;

    // rgb isn't needed because we can use setUsedAsColor when we create the
    // attribute

    if (iInterp == "vector")
    {
        if (iExtent == 2)
            attrStr = "vector2";
        // the data type makes it intrinsically a vector3
    }
    else if (iInterp == "point")
    {
        if (iExtent == 2)
            attrStr = "point2";
        // the data type is treated intrinsically as a point3
    }
    else if (iInterp == "normal")
    {
        if (iExtent == 2)
            attrStr = "normal2";
        else if (iExtent == 3)
            attrStr = "normal3";
    }

    if (attrStr != "")
    {
        std::string attrName = iAttrName + "_AbcType";
        addString(iParent, attrName, attrStr);
    }

    if (iScope != "" && iScope != "con")
    {
        std::string attrName = iAttrName + "_AbcGeomScope";
        addString(iParent, attrName, iScope);
    }
}

bool addProp(Alembic::Abc::IArrayProperty & iProp, MObject & iParent)
{
    MFnDependencyNode parentFn(iParent);
    MString attrName(iProp.getName().c_str());
    MPlug plug = parentFn.findPlug(attrName);

    MFnTypedAttribute typedAttr;
    MFnNumericAttribute numAttr;

    MObject attrObj;
    Alembic::AbcCoreAbstract::DataType dtype = iProp.getDataType();
    Alembic::Util::uint8_t extent = dtype.getExtent();
    std::string interp = iProp.getMetaData().get("interpretation");

    // the first sample is read only when the property is constant

    switch (dtype.getPod())
    {
        case Alembic::Util::kBooleanPOD:
        {
            if (extent != 1 || !iProp.isScalarLike())
            {
                return false;
            }

            bool bval = 0;

            if (iProp.isConstant())
            {
                Alembic::AbcCoreAbstract::ArraySamplePtr val;
                iProp.get(val);
                bval =
                    ((Alembic::Util::bool_t *)(val->getData()))[0] != false;
            }

            if (plug.isNull())
            {
                attrObj = numAttr.create(attrName, attrName,
                    MFnNumericData::kBoolean, bval);
            }
            else
            {
                plug.setValue(bval);
                return true;
            }
        }
        break;

        case Alembic::Util::kUint8POD:
        case Alembic::Util::kInt8POD:
        {
            if (extent != 1 || !iProp.isScalarLike())
            {
                return false;
            }

            // default is 1 just to accomodate visiblitity
            Alembic::Util::int8_t val = 1;

            if (iProp.isConstant())
            {
                Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                iProp.get(samp);
                val = ((Alembic::Util::int8_t *) samp->getData())[0];
            }

            if (plug.isNull())
            {
                attrObj = numAttr.create(attrName, attrName,
                    MFnNumericData::kByte, val);
            }
            else
            {
                plug.setValue(val);
            }
        }
        break;

        case Alembic::Util::kInt16POD:
        case Alembic::Util::kUint16POD:
        {
            // MFnNumericData::kShort or k2Short or k3Short
            if (extent > 3 || !iProp.isScalarLike())
            {
                return false;
            }

            Alembic::Util::int16_t val[3] = {0, 0, 0};

            if (iProp.isConstant())
            {
                Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                iProp.get(samp);
                const Alembic::Util::int16_t * sampData =
                    (const Alembic::Util::int16_t *) samp->getData();

                for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                {
                    val[i] = sampData[i];
                }
            }

            if (!plug.isNull())
            {
                unsigned int numChildren = plug.numChildren();
                if (numChildren == 0)
                {
                    plug.setValue(val[0]);
                }
                else
                {
                    if (numChildren > extent)
                        numChildren = extent;

                    for (unsigned int i = 0; i < numChildren; ++i)
                    {
                        plug.child(i).setValue(val[i]);
                    }
                }
                return true;
            }
            else if (extent == 1)
            {
                attrObj = numAttr.create(attrName, attrName,
                    MFnNumericData::kShort);
                numAttr.setDefault(val[0]);
            }
            else if (extent == 2)
            {
                attrObj = numAttr.create(attrName, attrName,
                    MFnNumericData::k2Short);
                numAttr.setDefault(val[0], val[1]);
            }
            else if (extent == 3)
            {
                attrObj = numAttr.create(attrName, attrName,
                    MFnNumericData::k3Short);
                numAttr.setDefault(val[0], val[1], val[2]);
            }
        }
        break;

        case Alembic::Util::kUint32POD:
        case Alembic::Util::kInt32POD:
        {
            if (!iProp.isScalarLike())
            {
                MFnIntArrayData fnData;
                MObject arrObj;

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);

                    MIntArray arr((int *) samp->getData(),
                        static_cast<unsigned int>(samp->size()));
                    arrObj = fnData.create(arr);
                    if (!plug.isNull())
                    {
                        plug.setValue(arrObj);
                        return true;
                    }
                }
                else
                {
                    MIntArray arr;
                    arrObj = fnData.create(arr);
                }

                attrObj = typedAttr.create(attrName, attrName,
                    MFnData::kIntArray, arrObj);
            }
            // isScalarLike
            else
            {
                if (extent > 3)
                {
                    return false;
                }

                Alembic::Util::int32_t val[3] = {0, 0, 0};

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);
                    const Alembic::Util::int32_t * sampData =
                        (const Alembic::Util::int32_t *)samp->getData();
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                        val[i] = sampData[i];
                    }
                }

                if (!plug.isNull())
                {
                    unsigned int numChildren = plug.numChildren();
                    if (numChildren == 0)
                    {
                        plug.setValue(val[0]);
                    }
                    else
                    {
                        if (numChildren > extent)
                            numChildren = extent;

                        for (unsigned int i = 0; i < numChildren; ++i)
                        {
                            plug.child(i).setValue(val[i]);
                        }
                    }
                    return true;
                }
                else if (extent == 1)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::kInt);
                    numAttr.setDefault(val[0]);
                }
                else if (extent == 2)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k2Int);
                    numAttr.setDefault(val[0], val[1]);
                }
                else if (extent == 3)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k3Int);
                    numAttr.setDefault(val[0], val[1], val[2]);
                }
            }
        }
        break;

        // look for MFnVectorArrayData?
        case Alembic::Util::kFloat32POD:
        {
            if (!iProp.isScalarLike())
            {
                if ((extent == 2 || extent == 3) && (interp == "normal" ||
                    interp == "vector" || interp == "rgb"))
                {
                    MFnVectorArrayData fnData;
                    MObject arrObj;

                    if (iProp.isConstant())
                    {
                        Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                        iProp.get(samp);

                        std::size_t sampSize = samp->size();
                        MVectorArray arr(sampSize);
                        MVector vec;
                        const Alembic::Util::float32_t * sampData =
                            (const Alembic::Util::float32_t *) samp->getData();

                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            vec.x = sampData[extent*i];
                            vec.y = sampData[extent*i+1];

                            if (extent == 3)
                            {
                                vec.z = sampData[extent*i+2];
                            }
                            arr[i] = vec;
                        }

                        arrObj = fnData.create(arr);
                        if (!plug.isNull())
                        {
                            plug.setValue(arrObj);
                            return true;
                        }
                    }
                    else
                    {
                        MVectorArray arr;
                        arrObj = fnData.create(arr);
                    }

                    attrObj = typedAttr.create(attrName, attrName,
                        MFnData::kVectorArray, arrObj);
                }
                else if (interp == "point" && (extent == 2 || extent == 3))
                {
                    MFnPointArrayData fnData;
                    MObject arrObj;

                    if (iProp.isConstant())
                    {
                        Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                        iProp.get(samp);

                        std::size_t sampSize = samp->size();
                        MPointArray arr(sampSize);
                        MPoint pt;

                        const Alembic::Util::float32_t * sampData =
                            (const Alembic::Util::float32_t *) samp->getData();

                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            pt.x = sampData[extent*i];
                            pt.y = sampData[extent*i+1];

                            if (extent == 3)
                            {
                                pt.z = sampData[extent*i+2];
                            }
                            arr[i] = pt;
                        }

                        arrObj = fnData.create(arr);
                        if (!plug.isNull())
                        {
                            plug.setValue(arrObj);
                            return true;
                        }
                    }
                    else
                    {
                        MPointArray arr;
                        arrObj = fnData.create(arr);
                    }

                    attrObj = typedAttr.create(attrName, attrName,
                        MFnData::kPointArray, arrObj);
                }
                else
                {
                    MFnDoubleArrayData fnData;
                    MObject arrObj;

                    if (iProp.isConstant())
                    {
                        Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                        iProp.get(samp);

                        MDoubleArray arr((float *) samp->getData(),
                            static_cast<unsigned int>(samp->size()));
                        arrObj = fnData.create(arr);
                        if (!plug.isNull())
                        {
                            plug.setValue(arrObj);
                            return true;
                        }
                    }
                    else
                    {
                        MDoubleArray arr;
                        arrObj = fnData.create(arr);
                    }

                    attrObj = typedAttr.create(attrName, attrName,
                        MFnData::kDoubleArray, arrObj);
                }

            }
            // isScalarLike
            else
            {
                if (extent > 3)
                {
                    return false;
                }

                float val[3] = {0, 0, 0};

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);
                    const float * sampData = (const float *) samp->getData();
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                        val[i] = sampData[i];
                    }
                }

                if (!plug.isNull())
                {
                    unsigned int numChildren = plug.numChildren();
                    if (numChildren == 0)
                    {
                        plug.setValue(val[0]);
                    }
                    else
                    {
                        if (numChildren > extent)
                            numChildren = extent;

                        for (unsigned int i = 0; i < numChildren; ++i)
                        {
                            plug.child(i).setValue(val[i]);
                        }
                    }
                    return true;
                }
                else if (extent == 1)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::kFloat);
                    numAttr.setDefault(val[0]);
                }
                else if (extent == 2)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k2Float);
                    numAttr.setDefault(val[0], val[1]);
                }
                else if (extent == 3)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k3Float);
                    numAttr.setDefault(val[0], val[1], val[2]);
                }
            }
        }
        break;

        case Alembic::Util::kFloat64POD:
        {
            if (!iProp.isScalarLike())
            {
                if ((extent == 2 || extent == 3) && (interp == "normal" ||
                    interp == "vector" || interp == "rgb"))
                {
                    MFnVectorArrayData fnData;
                    MObject arrObj;

                    if (iProp.isConstant())
                    {
                        Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                        iProp.get(samp);

                        std::size_t sampSize = samp->size();
                        MVectorArray arr(sampSize);
                        MVector vec;
                        const Alembic::Util::float64_t * sampData =
                            (const Alembic::Util::float64_t *) samp->getData();

                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            vec.x = sampData[extent*i];
                            vec.y = sampData[extent*i+1];

                            if (extent == 3)
                            {
                                vec.z = sampData[extent*i+2];
                            }

                            arr[i] = vec;
                        }

                        arrObj = fnData.create(arr);
                        if (!plug.isNull())
                        {
                            plug.setValue(arrObj);
                            return true;
                        }
                    }
                    else
                    {
                        MVectorArray arr;
                        arrObj = fnData.create(arr);
                    }

                    attrObj = typedAttr.create(attrName, attrName,
                        MFnData::kVectorArray, arrObj);
                }
                else if (interp == "point" && (extent == 2 || extent == 3))
                {
                    MFnPointArrayData fnData;
                    MObject arrObj;

                    if (iProp.isConstant())
                    {
                        Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                        iProp.get(samp);

                        std::size_t sampSize = samp->size();
                        MPointArray arr(sampSize);
                        MPoint pt;
                        const Alembic::Util::float64_t * sampData =
                            (const Alembic::Util::float64_t *) samp->getData();

                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            pt.x = sampData[extent*i];
                            pt.y = sampData[extent*i+1];

                            if (extent == 3)
                            {
                                pt.z = sampData[extent*i+2];
                            }

                            arr[i] = pt;
                        }

                        arrObj = fnData.create(arr);
                        if (!plug.isNull())
                        {
                            plug.setValue(arrObj);
                            return true;
                        }
                    }
                    else
                    {
                        MPointArray arr;
                        arrObj = fnData.create(arr);
                    }

                    attrObj = typedAttr.create(attrName, attrName,
                        MFnData::kPointArray, arrObj);
                }
                else
                {
                    MFnDoubleArrayData fnData;
                    MObject arrObj;

                    if (iProp.isConstant())
                    {
                        Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                        iProp.get(samp);

                        MDoubleArray arr((double *) samp->getData(),
                            static_cast<unsigned int>(samp->size()));
                        arrObj = fnData.create(arr);
                        if (!plug.isNull())
                        {
                            plug.setValue(arrObj);
                            return true;
                        }
                    }
                    else
                    {
                        MDoubleArray arr;
                        arrObj = fnData.create(arr);
                    }

                    attrObj = typedAttr.create(attrName, attrName,
                        MFnData::kDoubleArray, arrObj);
                }

            }
            else
            {
                if (extent > 4)
                {
                    return false;
                }

                double val[4] = {0, 0, 0, 0};

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);
                    const double * sampData = (const double *) samp->getData();
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                        val[i] = sampData[i];
                    }
                }

                if (!plug.isNull())
                {
                    unsigned int numChildren = plug.numChildren();
                    if (numChildren == 0)
                    {
                        plug.setValue(val[0]);
                    }
                    else
                    {
                        if (numChildren > extent)
                            numChildren = extent;

                        for (unsigned int i = 0; i < numChildren; ++i)
                        {
                            plug.child(i).setValue(val[i]);
                        }
                    }
                    return true;
                }
                else if (extent == 1)
                {
                    if (plug.isNull())
                    {
                        attrObj = numAttr.create(attrName, attrName,
                            MFnNumericData::kDouble);
                        numAttr.setDefault(val[0]);
                    }
                    else
                    {
                        plug.setValue(val[0]);
                        return true;
                    }
                }
                else if (extent == 2)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k2Double);
                    numAttr.setDefault(val[0], val[1]);
                }
                else if (extent == 3)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k3Double);
                    numAttr.setDefault(val[0], val[1], val[2]);
                }
                else if (extent == 4)
                {
                    attrObj = numAttr.create(attrName, attrName,
                        MFnNumericData::k4Double);
                    numAttr.setDefault(val[0], val[1], val[2], val[4]);
                }
            }
        }
        break;

        // MFnStringArrayData
        case Alembic::Util::kStringPOD:
        {
            if (!iProp.isScalarLike())
            {
                MFnStringArrayData fnData;
                MObject arrObj;

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);

                    std::size_t sampSize = samp->size();
                    MStringArray arr;
                    arr.setLength(sampSize);

                    Alembic::Util::string * strData =
                        (Alembic::Util::string *) samp->getData();

                    for (std::size_t i = 0; i < sampSize; ++i)
                    {
                        arr[i] = strData[i].c_str();
                    }
                    arrObj = fnData.create(arr);
                    if (!plug.isNull())
                    {
                        plug.setValue(arrObj);
                        return true;
                    }
                }
                else
                {
                    MStringArray arr;
                    arrObj = fnData.create(arr);
                }

                attrObj = typedAttr.create(attrName, attrName,
                    MFnData::kStringArray, arrObj);
            }
            // isScalarLike
            else
            {
                if (extent != 1)
                {
                    return false;
                }

                MFnStringData fnStringData;
                MObject strAttrObject;

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);
                    MString attrValue(
                        ((Alembic::Util::string *) samp->getData())[0].c_str());
                    strAttrObject = fnStringData.create(attrValue);
                    if (!plug.isNull())
                    {
                        plug.setValue(strAttrObject);
                        return true;
                    }
                }
                else
                {
                    MString attrValue;
                    strAttrObject = fnStringData.create(attrValue);
                }

                attrObj = typedAttr.create(attrName, attrName, MFnData::kString,
                    strAttrObject);
            }
        }
        break;

        // MFnStringArrayData
        case Alembic::Util::kWstringPOD:
        {
            if (!iProp.isScalarLike())
            {
                MFnStringArrayData fnData;
                MObject arrObj;

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);

                    std::size_t sampSize = samp->size();
                    MStringArray arr;
                    arr.setLength(sampSize);

                    Alembic::Util::wstring * strData =
                        (Alembic::Util::wstring *) samp->getData();

                    for (std::size_t i = 0; i < sampSize; ++i)
                    {
                        arr[i] = (wchar_t *)(strData[i].c_str());
                    }
                    arrObj = fnData.create(arr);

                    if (!plug.isNull())
                    {
                        plug.setValue(arrObj);
                        return true;
                    }
                }
                else
                {
                    MStringArray arr;
                    arrObj = fnData.create(arr);
                }

                attrObj = typedAttr.create(attrName, attrName,
                    MFnData::kStringArray, arrObj);
            }
            // isScalarLike
            else
            {
                if (extent != 1)
                {
                    return false;
                }

                MFnStringData fnStringData;
                MObject strAttrObject;

                if (iProp.isConstant())
                {
                    Alembic::AbcCoreAbstract::ArraySamplePtr samp;
                    iProp.get(samp);
                    MString attrValue(
                        ((Alembic::Util::wstring *)samp->getData())[0].c_str());
                    strAttrObject = fnStringData.create(attrValue);
                    if (!plug.isNull())
                    {
                        plug.setValue(strAttrObject);
                        return true;
                    }
                }
                else
                {
                    MString attrValue;
                    strAttrObject = fnStringData.create(attrValue);
                }

                attrObj = typedAttr.create(attrName, attrName, MFnData::kString,
                    strAttrObject);
            }
        }
        break;

        default:
        {
            // Not sure what to do with kFloat16POD, kInt64POD, kUInt64POD
            // so we'll just skip them for now
            return false;
        }
        break;
    }

    typedAttr.setKeyable(true);
    numAttr.setKeyable(true);

    if (interp == "rgb")
    {
        typedAttr.setUsedAsColor(true);
        numAttr.setUsedAsColor(true);
    }

    parentFn.addAttribute(attrObj,  MFnDependencyNode::kLocalDynamicAttr);
    addArbAttrAndScope(iParent, iProp.getName(),
        iProp.getMetaData().get("geoScope"), interp, extent);

    return true;
}

//=============================================================================


void addProps(Alembic::Abc::ICompoundProperty & iParent, MObject & iObject)
{
    // if the arbitrary geom params aren't valid, then skip
    if (!iParent)
        return;

    std::size_t numProps = iParent.getNumProperties();
    for (std::size_t i = 0; i < numProps; ++i)
    {
        const Alembic::Abc::PropertyHeader & propHeader =
            iParent.getPropertyHeader(i);

        const std::string & propName = propHeader.getName();

        if (!propHeader.isArray())
        {
            MString warn = "Skipping indexed or non-array property: ";
            warn += propName.c_str();

            printWarning(warn);
        }
        else if (propName.empty() || propName[0] == '.' ||
            propName.find('[') != std::string::npos)
        {
            MString warn = "Skipping oddly named property: ";
            warn += propName.c_str();

            printWarning(warn);
        }
        else
        {
            Alembic::Abc::IArrayProperty prop(iParent, propName);
            if (prop.getNumSamples() == 0)
            {
                MString warn = "Skipping property with no samples: ";
                warn += propName.c_str();

                printWarning(warn);
            }
            else if (!addProp(prop, iObject))
            {
                unsupportedWarning(prop);
            }
        }
    }
}

//=============================================================================

void getAnimatedProps(Alembic::Abc::ICompoundProperty & iParent,
    std::vector<Prop> & oPropList)
{
    // if the arbitrary geom params aren't valid, then skip
    if (!iParent)
        return;

    std::size_t numProps = iParent.getNumProperties();
    for (std::size_t i = 0; i < numProps; ++i)
    {
        const Alembic::Abc::PropertyHeader & propHeader =
            iParent.getPropertyHeader(i);
        const std::string & propName = propHeader.getName();

        if (!propHeader.isArray())
        {
            continue;
        }
        else if (propName.empty() || propName[0] == '.' ||
            propName.find('[') != std::string::npos)
        {
            continue;
        }

        Alembic::Abc::IArrayProperty prop(iParent, propName);
        if (prop.getNumSamples() == 0 || prop.isConstant())
        {
            continue;
        }

        Alembic::AbcCoreAbstract::DataType dtype = prop.getDataType();
        Alembic::Util::uint8_t extent = dtype.getExtent();

        switch (dtype.getPod())
        {
            case Alembic::Util::kBooleanPOD:
            case Alembic::Util::kUint8POD:
            case Alembic::Util::kInt8POD:
            {
                // we only support scalar bool, and int8
                if (extent != 1 || !prop.isScalarLike())
                {
                    continue;
                }

            }
            break;

            case Alembic::Util::kInt16POD:
            case Alembic::Util::kUint16POD:
            {
                // we only support scalar int16
                if (extent > 3 || !prop.isScalarLike())
                {
                    continue;
                }
            }
            break;

            case Alembic::Util::kUint32POD:
            case Alembic::Util::kInt32POD:
            case Alembic::Util::kFloat32POD:
            {
                if (prop.isScalarLike() && extent > 3)
                {
                    continue;
                }
            }
            break;

            case Alembic::Util::kFloat64POD:
            {
                if (prop.isScalarLike() && extent > 4)
                {
                    continue;
                }
            }
            break;

            // MFnStringArrayData
            case Alembic::Util::kStringPOD:
            case Alembic::Util::kWstringPOD:
            {
                if (prop.isScalarLike() && extent > 1)
                {
                    continue;
                }
            }
            break;

            default:
            {
                // Not sure what to do with kFloat16POD, kInt64POD, kUInt64POD
                // so we'll just skip them for now
                continue;
            }
            break;
        }

        Prop animProp;
        animProp.mArray = prop;
        oPropList.push_back(animProp);
    } // for i
}

//=============================================================================

void readProp(double iFrame, Alembic::Abc::IArrayProperty & iProp,
    MDataHandle & iHandle)
{
    MObject attrObj;
    Alembic::AbcCoreAbstract::DataType dtype = iProp.getDataType();
    Alembic::Util::uint8_t extent = dtype.getExtent();

    Alembic::AbcCoreAbstract::ArraySamplePtr samp, ceilSamp;

    Alembic::AbcCoreAbstract::index_t index, ceilIndex;
    double alpha = getWeightAndIndex(iFrame, iProp.getTimeSampling(),
        iProp.getNumSamples(), index, ceilIndex);

    switch (dtype.getPod())
    {
        case Alembic::Util::kBooleanPOD:
        {
            if (!iProp.isScalarLike() || extent != 1)
            {
                return;
            }

            iProp.get(samp, index);
            Alembic::Util::bool_t val =
                ((Alembic::Util::bool_t *) samp->getData())[0];
            iHandle.setGenericBool(val != false, false);
        }
        break;

        case Alembic::Util::kUint8POD:
        case Alembic::Util::kInt8POD:
        {
            if (!iProp.isScalarLike() || extent != 1)
            {
                return;
            }

            Alembic::Util::int8_t val;

            if (index != ceilIndex && alpha != 0.0)
            {
                iProp.get(samp, index);
                iProp.get(ceilSamp, ceilIndex);
                Alembic::Util::int8_t lo =
                    ((Alembic::Util::int8_t *) samp->getData())[0];
                Alembic::Util::int8_t hi =
                    ((Alembic::Util::int8_t *) ceilSamp->getData())[0];
                val = simpleLerp<Alembic::Util::int8_t>(alpha, lo, hi);
            }
            else
            {
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                val = ((Alembic::Util::int8_t *) samp->getData())[0];
            }

            iHandle.setGenericChar(val, false);
        }
        break;

        case Alembic::Util::kInt16POD:
        case Alembic::Util::kUint16POD:
        {
            Alembic::Util::int16_t val[3];

            if (index != ceilIndex && alpha != 0.0)
            {
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                iProp.get(ceilSamp, Alembic::Abc::ISampleSelector(ceilIndex));
                for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                {
                     val[i] = simpleLerp<Alembic::Util::int16_t>(alpha,
                        ((Alembic::Util::int16_t *)samp->getData())[i],
                        ((Alembic::Util::int16_t *)ceilSamp->getData())[i]);
                }
            }
            else
            {
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                {
                     val[i] = ((Alembic::Util::int16_t *)samp->getData())[i];
                }
            }

            if (extent == 1)
            {
                iHandle.setGenericShort(val[0], false);
            }
            else if (extent == 2)
            {
                MFnNumericData numData;
                numData.create(MFnNumericData::k2Short);
                numData.setData2Short(val[0], val[1]);
                iHandle.setMObject(numData.object());
            }
            else if (extent == 3)
            {
                MFnNumericData numData;
                numData.create(MFnNumericData::k3Short);
                numData.setData3Short(val[0], val[1], val[2]);
                iHandle.setMObject(numData.object());
            }
        }
        break;

        case Alembic::Util::kUint32POD:
        case Alembic::Util::kInt32POD:
        {
            if (iProp.isScalarLike() && extent < 4)
            {
                Alembic::Util::int32_t val[3];

                if (index != ceilIndex && alpha != 0.0)
                {
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    iProp.get(ceilSamp,
                        Alembic::Abc::ISampleSelector(ceilIndex));
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                         val[i] = simpleLerp<Alembic::Util::int32_t>(alpha,
                            ((Alembic::Util::int32_t *)samp->getData())[i],
                            ((Alembic::Util::int32_t *)ceilSamp->getData())[i]);
                    }
                }
                else
                {
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                         val[i] =
                            ((Alembic::Util::int32_t *) samp->getData())[i];
                    }
                }

                if (extent == 1)
                {
                    iHandle.setGenericInt(val[0], false);
                }
                else if (extent == 2)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k2Int);
                    numData.setData2Int(val[0], val[1]);
                    iHandle.setMObject(numData.object());
                }
                else if (extent == 3)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k3Int);
                    numData.setData3Int(val[0], val[1], val[2]);
                    iHandle.setMObject(numData.object());
                }
            }
            else
            {
                MFnIntArrayData fnData;
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                if (alpha != 0.0 && index != ceilIndex)
                {
                    iProp.get(ceilSamp,
                        Alembic::Abc::ISampleSelector(ceilIndex));

                    MIntArray arr((int *) samp->getData(),
                        static_cast<unsigned int>(samp->size()));
                    std::size_t sampSize = samp->size();

                    // size is different don't lerp
                    if (sampSize != ceilSamp->size())
                    {
                        attrObj = fnData.create(arr);
                    }
                    else
                    {
                        int * hi = (int *) ceilSamp->getData();
                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            arr[i] = simpleLerp<int>(alpha, arr[i], hi[i]);
                        }
                    }
                    attrObj = fnData.create(arr);
                }
                else
                {
                    MIntArray arr((int *) samp->getData(),
                        static_cast<unsigned int>(samp->size()));
                    attrObj = fnData.create(arr);
                }
            }
        }
        break;

        case Alembic::Util::kFloat32POD:
        {
            if (iProp.isScalarLike() && extent < 4)
            {
                float val[3];

                if (index != ceilIndex && alpha != 0.0)
                {
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    iProp.get(ceilSamp,
                        Alembic::Abc::ISampleSelector(ceilIndex));
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                        val[i] = simpleLerp<float>(alpha,
                            ((float *)samp->getData())[i],
                            ((float *)ceilSamp->getData())[i]);
                    }
                }
                else
                {
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                        val[i] = ((float *)samp->getData())[i];
                    }
                }

                if (extent == 1)
                {
                    iHandle.setGenericFloat(val[0], false);
                }
                else if (extent == 2)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k2Float);
                    numData.setData2Float(val[0], val[1]);
                    iHandle.setMObject(numData.object());
                }
                else if (extent == 3)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k3Float);
                    numData.setData3Float(val[0], val[1], val[2]);
                    iHandle.setMObject(numData.object());
                }
            }
            else
            {
                std::string interp = iProp.getMetaData().get("interpretation");

                if ((extent == 2 || extent == 3) && (interp == "normal" ||
                    interp == "vector" || interp == "rgb"))
                {
                    MFnVectorArrayData fnData;
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    std::size_t sampSize = samp->size();
                    MVectorArray arr(sampSize);
                    MVector vec;

                    if (alpha != 0.0 && index != ceilIndex)
                    {
                        iProp.get(ceilSamp,
                            Alembic::Abc::ISampleSelector(ceilIndex));

                        // size is different don't lerp
                        if (sampSize != ceilSamp->size())
                        {
                            float * vals = (float *) samp->getData();
                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                vec.x = vals[extent*i];
                                vec.y = vals[extent*i+1];

                                if (extent == 3)
                                {
                                    vec.z = vals[extent*i+2];
                                }
                                arr[i] = vec;
                            }

                            attrObj = fnData.create(arr);
                        }
                        else
                        {
                            float * lo = (float *) samp->getData();
                            float * hi = (float *) ceilSamp->getData();

                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                vec.x = simpleLerp<double>(alpha,
                                    lo[extent*i], hi[extent*i]);

                                vec.y = simpleLerp<double>(alpha,
                                    lo[extent*i+1], hi[extent*i+1]);

                                if (extent == 3)
                                {
                                    vec.z = simpleLerp<double>(alpha,
                                        lo[extent*i+2], hi[extent*i+2]);
                                }
                                arr[i] = vec;
                            }
                        }
                        attrObj = fnData.create(arr);
                    }
                    else
                    {
                        float * vals = (float *) samp->getData();
                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            vec.x = vals[extent*i];
                            vec.y = vals[extent*i+1];

                            if (extent == 3)
                            {
                                vec.z = vals[extent*i+2];
                            }
                            arr[i] = vec;
                        }
                        attrObj = fnData.create(arr);
                    }
                }
                else if (interp == "point" && (extent == 2 || extent == 3))
                {
                    MFnPointArrayData fnData;
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                    std::size_t sampSize = samp->size();
                    MPointArray arr(sampSize);
                    MPoint pt;

                    if (alpha != 0.0 && index != ceilIndex)
                    {
                        iProp.get(ceilSamp,
                            Alembic::Abc::ISampleSelector(ceilIndex));

                        // size is different don't lerp
                        if (sampSize != ceilSamp->size())
                        {
                            float * vals = (float *) samp->getData();
                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                pt.x = vals[extent*i];
                                pt.y = vals[extent*i+1];

                                if (extent == 3)
                                {
                                    pt.z = vals[extent*i+2];
                                }
                                arr[i] = pt;
                            }

                            attrObj = fnData.create(arr);
                        }
                        else
                        {
                            float * lo = (float *) samp->getData();
                            float * hi = (float *) ceilSamp->getData();

                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                pt.x = simpleLerp<double>(alpha,
                                    lo[extent*i], hi[extent*i]);

                                pt.y = simpleLerp<double>(alpha,
                                    lo[extent*i+1], hi[extent*i+1]);

                                if (extent == 3)
                                {
                                    pt.z = simpleLerp<double>(alpha,
                                        lo[extent*i+2], hi[extent*i+2]);
                                }
                                arr[i] = pt;
                            }
                            attrObj = fnData.create(arr);
                        }
                    }
                    else
                    {
                        float * vals = (float *) samp->getData();
                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            pt.x = vals[extent*i];
                            pt.y = vals[extent*i+1];

                            if (extent == 3)
                            {
                                pt.z = vals[extent*i+2];
                            }
                            arr[i] = pt;
                        }
                        attrObj = fnData.create(arr);
                    }
                }
                else
                {
                    MFnDoubleArrayData fnData;
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                    if (alpha != 0.0 && index != ceilIndex)
                    {
                        iProp.get(ceilSamp,
                            Alembic::Abc::ISampleSelector(ceilIndex));

                        MDoubleArray arr((float *) samp->getData(),
                            static_cast<unsigned int>(samp->size()));
                        std::size_t sampSize = samp->size();

                        // size is different don't lerp
                        if (sampSize != ceilSamp->size())
                        {
                            attrObj = fnData.create(arr);
                        }
                        else
                        {
                            float * hi = (float *) ceilSamp->getData();
                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                arr[i] = simpleLerp<double>(alpha, arr[i],
                                    hi[i]);
                            }
                        }
                        attrObj = fnData.create(arr);
                    }
                    else
                    {
                        MDoubleArray arr((float *) samp->getData(),
                            static_cast<unsigned int>(samp->size()));
                        attrObj = fnData.create(arr);
                    }
                }
            }
        }
        break;

        case Alembic::Util::kFloat64POD:
        {
            // need to differentiate between vectors, points, and color array?
            if (iProp.isScalarLike() && extent < 5)
            {
                double val[4];

                if (index != ceilIndex && alpha != 0.0)
                {
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                    iProp.get(ceilSamp,
                        Alembic::Abc::ISampleSelector(ceilIndex));
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                         val[i] = simpleLerp<double>(alpha,
                            ((double *)(samp->getData()))[i],
                            ((double *)(ceilSamp->getData()))[i]);
                    }
                }
                else
                {
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    for (Alembic::Util::uint8_t i = 0; i < extent; ++i)
                    {
                         val[i] = ((double *)(samp->getData()))[i];
                    }
                }

                if (extent == 1)
                {
                    iHandle.setGenericDouble(val[0], false);
                }
                else if (extent == 2)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k2Double);
                    numData.setData2Double(val[0], val[1]);
                    iHandle.setMObject(numData.object());
                }
                else if (extent == 3)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k3Double);
                    numData.setData3Double(val[0], val[1], val[2]);
                    iHandle.setMObject(numData.object());
                }
                else if (extent == 4)
                {
                    MFnNumericData numData;
                    numData.create(MFnNumericData::k4Double);
                    numData.setData4Double(val[0], val[1], val[2], val[3]);
                    iHandle.setMObject(numData.object());
                }
            }
            else
            {
                std::string interp = iProp.getMetaData().get("interpretation");

                if ((extent == 2 || extent == 3) && (interp == "normal" ||
                    interp == "vector" || interp == "rgb"))
                {
                    MFnVectorArrayData fnData;
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    std::size_t sampSize = samp->size();
                    MVectorArray arr(sampSize);
                    MVector vec;

                    if (alpha != 0.0 && index != ceilIndex)
                    {
                        iProp.get(ceilSamp,
                            Alembic::Abc::ISampleSelector(ceilIndex));

                        // size is different don't lerp
                        if (sampSize != ceilSamp->size())
                        {
                            double * vals = (double *) samp->getData();
                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                vec.x = vals[extent*i];
                                vec.y = vals[extent*i+1];

                                if (extent == 3)
                                {
                                    vec.z = vals[extent*i+2];
                                }
                                arr[i] = vec;
                            }

                            attrObj = fnData.create(arr);
                        }
                        else
                        {
                            double * lo = (double *) samp->getData();
                            double * hi = (double *) ceilSamp->getData();

                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                vec.x = simpleLerp<double>(alpha,
                                    lo[extent*i], hi[extent*i]);

                                vec.y = simpleLerp<double>(alpha,
                                    lo[extent*i+1], hi[extent*i+1]);

                                if (extent == 3)
                                {
                                    vec.z = simpleLerp<double>(alpha,
                                        lo[extent*i+2], hi[extent*i+2]);
                                }
                                arr[i] = vec;
                            }
                        }
                        attrObj = fnData.create(arr);
                    }
                    else
                    {
                        double * vals = (double *) samp->getData();
                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            vec.x = vals[extent*i];
                            vec.y = vals[extent*i+1];

                            if (extent == 3)
                            {
                                vec.z = vals[extent*i+2];
                            }
                            arr[i] = vec;
                        }
                        attrObj = fnData.create(arr);
                    }
                }
                else if (interp == "point" && (extent == 2 || extent == 3))
                {
                    MFnPointArrayData fnData;
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                    std::size_t sampSize = samp->size();
                    MPointArray arr(sampSize);
                    MPoint pt;

                    if (alpha != 0.0 && index != ceilIndex)
                    {
                        iProp.get(ceilSamp,
                            Alembic::Abc::ISampleSelector(ceilIndex));

                        // size is different don't lerp
                        if (sampSize != ceilSamp->size())
                        {
                            double * vals = (double *) samp->getData();
                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                pt.x = vals[extent*i];
                                pt.y = vals[extent*i+1];

                                if (extent == 3)
                                {
                                    pt.z = vals[extent*i+2];
                                }
                                arr[i] = pt;
                            }

                            attrObj = fnData.create(arr);
                        }
                        else
                        {
                            double * lo = (double *) samp->getData();
                            double * hi = (double *) ceilSamp->getData();

                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                pt.x = simpleLerp<double>(alpha,
                                    lo[extent*i], hi[extent*i]);

                                pt.y = simpleLerp<double>(alpha,
                                    lo[extent*i+1], hi[extent*i+1]);

                                if (extent == 3)
                                {
                                    pt.z = simpleLerp<double>(alpha,
                                        lo[extent*i+2], hi[extent*i+2]);
                                }
                                arr[i] = pt;
                            }
                            attrObj = fnData.create(arr);
                        }
                    }
                    else
                    {
                        double * vals = (double *) samp->getData();
                        for (std::size_t i = 0; i < sampSize; ++i)
                        {
                            pt.x = vals[extent*i];
                            pt.y = vals[extent*i+1];

                            if (extent == 3)
                            {
                                pt.z = vals[extent*i+2];
                            }
                            arr[i] = pt;
                        }
                        attrObj = fnData.create(arr);
                    }
                }
                else
                {
                    MFnDoubleArrayData fnData;
                    iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                    if (alpha != 0.0 && index != ceilIndex)
                    {
                        iProp.get(ceilSamp,
                            Alembic::Abc::ISampleSelector(ceilIndex));

                        MDoubleArray arr((double *) samp->getData(),
                            static_cast<unsigned int>(samp->size()));
                        std::size_t sampSize = samp->size();

                        // size is different don't lerp
                        if (sampSize != ceilSamp->size())
                        {
                            attrObj = fnData.create(arr);
                        }
                        else
                        {
                            double * hi = (double *) ceilSamp->getData();
                            for (std::size_t i = 0; i < sampSize; ++i)
                            {
                                arr[i] = simpleLerp<double>(alpha, arr[i],
                                    hi[i]);
                            }
                        }
                        attrObj = fnData.create(arr);
                    }
                    else
                    {
                        MDoubleArray arr((double *) samp->getData(),
                            static_cast<unsigned int>(samp->size()));
                        attrObj = fnData.create(arr);
                    }
                }
            }
        }
        break;

        // MFnStringArrayData
        case Alembic::Util::kStringPOD:
        {
            if (iProp.isScalarLike() && extent == 1)
            {
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                iHandle.setString(
                    ((Alembic::Util::string *)samp->getData())[0].c_str());
            }
            else
            {
                MFnStringArrayData fnData;
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                std::size_t sampSize = samp->size();
                MStringArray arr;
                arr.setLength(sampSize);
                attrObj = fnData.create(arr);
                Alembic::Util::string * strData =
                    (Alembic::Util::string *) samp->getData();

                for (std::size_t i = 0; i < sampSize; ++i)
                {
                    arr[i] = strData[i].c_str();
                }
            }
        }
        break;

        // MFnStringArrayData
        case Alembic::Util::kWstringPOD:
        {
            if (iProp.isScalarLike() && extent == 1)
            {
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));
                iHandle.setString(
                    ((Alembic::Util::wstring *)samp->getData())[0].c_str());
            }
            else
            {
                MFnStringArrayData fnData;
                iProp.get(samp, Alembic::Abc::ISampleSelector(index));

                std::size_t sampSize = samp->size();
                MStringArray arr;
                arr.setLength(sampSize);
                attrObj = fnData.create(arr);
                Alembic::Util::wstring * strData =
                    (Alembic::Util::wstring *) samp->getData();

                for (std::size_t i = 0; i < sampSize; ++i)
                {
                    arr[i] = (wchar_t *)strData[i].c_str();
                }
            }
        }
        break;

        default:
        break;
    }

    if (!attrObj.isNull())
        iHandle.set(attrObj);
}

WriterData::WriterData()
{
}

WriterData::~WriterData()
{
    // prop
    mPropList.clear();
}

WriterData::WriterData(const WriterData & rhs)
{
    *this = rhs;
}

WriterData & WriterData::operator=(const WriterData & rhs)
{

    mCameraList = rhs.mCameraList;
    mCurvesList = rhs.mCurvesList;
    mNurbsList = rhs.mNurbsList;
    mPointsList = rhs.mPointsList;
    mPolyMeshList = rhs.mPolyMeshList;
    mSubDList = rhs.mSubDList;
    mXformList = rhs.mXformList;
    mPropList = rhs.mPropList;
    mLocList = rhs.mLocList;
    mAnimVisStaticObjList = rhs.mAnimVisStaticObjList;

    // get all the sampled Maya objects
    mCameraObjList = rhs.mCameraObjList;
    mNurbsCurveObjList = rhs.mNurbsCurveObjList;
    mNurbsObjList = rhs.mNurbsObjList;
    mPointsObjList = rhs.mPointsObjList;
    mPolyMeshObjList = rhs.mPolyMeshObjList;
    mSubDObjList = rhs.mSubDObjList;
    mXformOpList = rhs.mXformOpList;
    mPropObjList = rhs.mPropObjList;
    mIsComplexXform = rhs.mIsComplexXform;
    mLocObjList = rhs.mLocObjList;

    mNumCurves = rhs.mNumCurves;

    return *this;
}

void WriterData::getFrameRange(double & oMin, double & oMax)
{
    oMin = DBL_MAX;
    oMax = -DBL_MAX;

    Alembic::AbcCoreAbstract::TimeSamplingPtr ts;

    std::size_t i = 0;
    std::size_t iEnd = mLocList.size();
    for (i = 0; i < iEnd; ++i)
    {
        Alembic::Abc::IScalarProperty locProp(mLocList[i].getProperties(), "locator");
        ts = locProp.getTimeSampling();
        std::size_t numSamples = locProp.getNumSamples();
        oMin = std::min(ts->getSampleTime(0), oMin);
        oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
    }

    iEnd = mPointsList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mPointsList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mPointsList[i].getSchema().getNumSamples();
        oMin = std::min(ts->getSampleTime(0), oMin);
        oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
    }

    iEnd = mPolyMeshList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mPolyMeshList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mPolyMeshList[i].getSchema().getNumSamples();
        oMin = std::min(ts->getSampleTime(0), oMin);
        oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
    }

    iEnd = mSubDList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mSubDList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mSubDList[i].getSchema().getNumSamples();
        oMin = std::min(ts->getSampleTime(0), oMin);
        oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
    }

    iEnd = mXformList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mXformList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mXformList[i].getSchema().getNumSamples();
        if (numSamples > 1)
        {
            oMin = std::min(ts->getSampleTime(0), oMin);
            oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
        }
    }

    iEnd = mCameraList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mCameraList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mCameraList[i].getSchema().getNumSamples();
        if (numSamples > 1)
        {
            oMin = std::min(ts->getSampleTime(0), oMin);
            oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
        }
    }

    iEnd = mCurvesList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mCurvesList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mCurvesList[i].getSchema().getNumSamples();
        if (numSamples > 1)
        {
            oMin = std::min(ts->getSampleTime(0), oMin);
            oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
        }
    }

    iEnd = mNurbsList.size();
    for (i = 0; i < iEnd; ++i)
    {
        ts = mNurbsList[i].getSchema().getTimeSampling();
        std::size_t numSamples = mNurbsList[i].getSchema().getNumSamples();
        if (numSamples > 1)
        {
            oMin = std::min(ts->getSampleTime(0), oMin);
            oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
        }
    }

    iEnd = mPropList.size();
    for (i = 0; i < iEnd; ++i)
    {
        std::size_t numSamples = 0;
        if (mPropList[i].mArray.valid())
        {
            ts = mPropList[i].mArray.getTimeSampling();
            numSamples = mPropList[i].mArray.getNumSamples();
        }
        else
        {
            ts = mPropList[i].mScalar.getTimeSampling();
            numSamples = mPropList[i].mScalar.getNumSamples();
        }

        if (numSamples > 1)
        {
            oMin = std::min(ts->getSampleTime(0), oMin);
            oMax = std::max(ts->getSampleTime(numSamples-1), oMax);
        }
    }
}

ArgData::ArgData(MString iFileName,
    bool iDebugOn, MObject iReparentObj, bool iConnect,
    MString iConnectRootNodes, bool iCreateIfNotFound, bool iRemoveIfNoUpdate) :
        mFileName(iFileName),
        mDebugOn(iDebugOn), mReparentObj(iReparentObj), mConnect(iConnect),
        mConnectRootNodes(iConnectRootNodes),
        mCreateIfNotFound(iCreateIfNotFound),
        mRemoveIfNoUpdate(iRemoveIfNoUpdate)
{
    mSequenceStartTime = -DBL_MAX;
    mSequenceEndTime = DBL_MAX;
}

ArgData::ArgData(const ArgData & rhs)
{
    *this = rhs;
}

ArgData & ArgData::operator=(const ArgData & rhs)
{
    mFileName = rhs.mFileName;
    mSequenceStartTime = rhs.mSequenceStartTime;
    mSequenceEndTime = rhs.mSequenceEndTime;

    mDebugOn = rhs.mDebugOn;

    mReparentObj = rhs.mReparentObj;

    // optional information for the "connect" flag
    mConnect = rhs.mConnect;
    mConnectRootNodes = rhs.mConnectRootNodes;
    mCreateIfNotFound = rhs.mCreateIfNotFound;
    mRemoveIfNoUpdate = rhs.mRemoveIfNoUpdate;

    mData = rhs.mData;

    return *this;
}

MString createScene(ArgData & iArgData)
{
    MString returnName("");

    // no caching!
    Alembic::Abc::IArchive archive(Alembic::AbcCoreHDF5::ReadArchive(),
        iArgData.mFileName.asChar(), Alembic::Abc::ErrorHandler::Policy(),
        Alembic::AbcCoreAbstract::ReadArraySampleCachePtr());
    if (!archive.valid())
    {
        MString theError = iArgData.mFileName;
        theError += MString(" not a valid Alembic file.");
        printError(theError);
        return returnName;
    }


    CreateSceneVisitor::Action action = CreateSceneVisitor::CREATE;
    if (iArgData.mRemoveIfNoUpdate && iArgData.mCreateIfNotFound)
        action = CreateSceneVisitor::CREATE_REMOVE;
    else if (iArgData.mRemoveIfNoUpdate)
        action = CreateSceneVisitor::REMOVE;
    else if (iArgData.mCreateIfNotFound)
        action = CreateSceneVisitor::CREATE;
    else if (iArgData.mConnect)
        action = CreateSceneVisitor::CONNECT;

    CreateSceneVisitor visitor(iArgData.mSequenceStartTime,
        iArgData.mReparentObj, action, iArgData.mConnectRootNodes);

    visitor.walk(archive);

    if (visitor.hasSampledData())
    {
        visitor.getData(iArgData.mData);

        iArgData.mData.getFrameRange(iArgData.mSequenceStartTime,
            iArgData.mSequenceEndTime);

        returnName = connectAttr(iArgData);
    }

    if (iArgData.mConnect)
    {
        visitor.applyShaderSelection();
    }

    return returnName;
}

MString connectAttr(ArgData & iArgData)
{
    MStatus status = MS::kSuccess;

    // create a new AlembicNode and initialize all its input attributes
    MDGModifier modifier;
    MPlug srcPlug, dstPlug;

    MObject alembicNodeObj = modifier.createNode("AlembicNode", &status);
    MFnDependencyNode alembicNodeFn(alembicNodeObj, &status);

    AlembicNode *alembicNodePtr =
        reinterpret_cast<AlembicNode*>(alembicNodeFn.userNode(&status));
    if (status == MS::kSuccess)
    {
        alembicNodePtr->setReaderPtrList(iArgData.mData);
        alembicNodePtr->setDebugMode(iArgData.mDebugOn);
    }

    // set AlembicNode name
    MString fileName;
    stripFileName(iArgData.mFileName, fileName);
    MString alembicNodeName = fileName +"_AlembicNode";
    alembicNodeFn.setName(alembicNodeName, &status);

    // set input file name
    MPlug plug = alembicNodeFn.findPlug("abc_File", true, &status);
    plug.setValue(iArgData.mFileName);

    // set sequence start and end in frames
    MTime sec(1.0, MTime::kSeconds);
    plug = alembicNodeFn.findPlug("startFrame", true, &status);
    plug.setValue(iArgData.mSequenceStartTime * sec.as(MTime::uiUnit()));

    plug = alembicNodeFn.findPlug("endFrame", true, &status);
    plug.setValue(iArgData.mSequenceEndTime * sec.as(MTime::uiUnit()));

    // set connect input info
    plug = alembicNodeFn.findPlug("connect", true, &status);
    plug.setValue(iArgData.mConnect);
    plug = alembicNodeFn.findPlug("createIfNotFound", true, &status);
    plug.setValue(iArgData.mCreateIfNotFound);
    plug = alembicNodeFn.findPlug("removeIfNoUpdate", true, &status);
    plug.setValue(iArgData.mRemoveIfNoUpdate);
    plug = alembicNodeFn.findPlug("connectRoot", true, &status);
    plug.setValue(iArgData.mConnectRootNodes);

    MFnIntArrayData fnIntArray;
    fnIntArray.create();
    MObject intArrayObj;
    MIntArray intArray;

    // make connection: time1.outTime --> alembicNode.intime
    dstPlug = alembicNodeFn.findPlug("time", true, &status);
    status = getPlugByName("time1", "outTime", srcPlug);
    status = modifier.connect(srcPlug, dstPlug);
    status = modifier.doIt();

    std::size_t subDSize       = iArgData.mData.mSubDObjList.size();
    std::size_t polySize       = iArgData.mData.mPolyMeshObjList.size();
    std::size_t cameraSize     = iArgData.mData.mCameraObjList.size();
    std::size_t particleSize   = iArgData.mData.mPointsObjList.size();
    std::size_t xformSize      = iArgData.mData.mXformOpList.size();
    std::size_t nSurfaceSize   = iArgData.mData.mNurbsObjList.size();
    std::size_t nCurveSize     = iArgData.mData.mNurbsCurveObjList.size();
    std::size_t propSize       = iArgData.mData.mPropObjList.size();
    std::size_t locatorSize    = iArgData.mData.mLocObjList.size();

    // making dynamic connections
    if (particleSize > 0)
    {
        printWarning("Currently no support for animated particle system");
    }
    if (locatorSize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("outLoc", true);

        unsigned int logicalIndex = 0;
        for (unsigned int i = 0; i < locatorSize; i++)
        {
            MFnDagNode fnLocator(iArgData.mData.mLocObjList[i]);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnLocator.findPlug("localPositionX", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnLocator.findPlug("localPositionY", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnLocator.findPlug("localPositionZ", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnLocator.findPlug("localScaleX", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnLocator.findPlug("localScaleY", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnLocator.findPlug("localScaleZ", true);
            modifier.connect(srcPlug, dstPlug);

            status = modifier.doIt();
        }
    }
    if (cameraSize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("outCamera", true);

        unsigned int logicalIndex = 0;
        for (unsigned int i = 0; i < cameraSize; i++)
        {
            MFnCamera fnCamera(iArgData.mData.mCameraObjList[i]);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("focalLength", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("lensSqueezeRatio", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("horizontalFilmAperture", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("verticalFilmAperture", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("horizontalFilmOffset", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("verticalFilmOffset", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("overscan", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("nearClipPlane", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("farClipPlane", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("fStop", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("focusDistance", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("shutterAngle", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("filmFitOffset", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("preScale", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug  = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("filmTranslateH", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("filmTranslateV", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("postScale", true);
            modifier.connect(srcPlug, dstPlug);

            srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);
            dstPlug = fnCamera.findPlug("cameraScale", true);
            modifier.connect(srcPlug, dstPlug);

            status = modifier.doIt();
        }
    }
    if (nSurfaceSize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("outNSurface", true);
        for (unsigned int i = 0; i < nSurfaceSize; i++)
        {
            srcPlug = srcArrayPlug.elementByLogicalIndex(i);
            MFnNurbsSurface fnNSurface(iArgData.mData.mNurbsObjList[i]);
            dstPlug = fnNSurface.findPlug("create", true);
            modifier.connect(srcPlug, dstPlug);
            status = modifier.doIt();
        }
    }
    if (nCurveSize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("outNCurveGrp", true);
        for (unsigned int i = 0; i < nCurveSize; i++)
        {
            srcPlug = srcArrayPlug.elementByLogicalIndex(i);
            MFnNurbsCurve fnNCurve(iArgData.mData.mNurbsCurveObjList[i]);
            dstPlug = fnNCurve.findPlug("create", true);
            modifier.connect(srcPlug, dstPlug);
            status = modifier.doIt();
        }
    }

    if (propSize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("prop", true, &status);
        unsigned int index = 0;
        for (unsigned int i = 0 ; i < propSize; i++)
        {
            SampledPair & sampPair = iArgData.mData.mPropObjList[i];
            MObject obj = sampPair.getObject();
            MFnDependencyNode mFn(obj);

            unsigned int sampleSize = sampPair.sampledChannelSize();
            for (unsigned int j = 0; j < sampleSize; j ++)
            {
                std::string attrName = sampPair.getSampleElement(j);
                if (attrName == Alembic::AbcGeom::kVisibilityPropertyName)
                    dstPlug = mFn.findPlug("visibility", true, &status);
                else
                    dstPlug = mFn.findPlug(attrName.c_str(), true, &status);

                if (attrName != Alembic::AbcGeom::kVisibilityPropertyName &&
                    (status != MS::kSuccess ||
                    dstPlug.partialName(false, false, false, false, false, true)
                    != attrName.c_str()))
                {
                    MString theError(attrName.c_str());
                    theError += MString(" not found for connection");
                    printError(theError);
                    continue;
                }

                srcPlug = srcArrayPlug.elementByLogicalIndex(index++);

                if (!dstPlug.isConnected())
                {
                    status = modifier.connect(srcPlug, dstPlug);
                    status = modifier.doIt();
                }

                if (status != MS::kSuccess)
                {
                    MString theError(srcPlug.name());
                    theError += MString(" --> ");
                    theError += dstPlug.name();
                    theError += MString(" connection not made");
                    printError(theError);
                }
            }
        }
    }

    if (subDSize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("outSubDMesh", true);
        for (unsigned int i = 0; i < subDSize; i++)
        {
            srcPlug = srcArrayPlug.elementByLogicalIndex(i);
            MFnMesh mFn(iArgData.mData.mSubDObjList[i], &status);
            dstPlug = mFn.findPlug("inMesh", true, &status);
            status = modifier.connect(srcPlug, dstPlug);
            status = modifier.doIt();
            if (status != MS::kSuccess)
            {
                MString theError("AlembicNode.outSubDMesh[");
                theError += i;
                theError += "] --> ";
                theError += mFn.name();
                theError += ".inMesh connection not made";
                printError(theError);
            }
        }
    }

    if (polySize > 0)
    {
        MPlug srcArrayPlug = alembicNodeFn.findPlug("outPolyMesh", true);
        for (unsigned int i = 0; i < polySize; i++)
        {
            srcPlug = srcArrayPlug.elementByLogicalIndex(i);
            MFnMesh mFn(iArgData.mData.mPolyMeshObjList[i]);
            dstPlug = mFn.findPlug("inMesh", true);
            modifier.connect(srcPlug, dstPlug);
            status = modifier.doIt();
        }
    }

    if (xformSize > 0)
    {
        unsigned int logicalIndex = 0;
        MPlug srcArrayPlug = alembicNodeFn.findPlug("transOp", true);

        for (unsigned int i = 0 ; i < xformSize; i++)
        {
            SampledPair & sampPair = iArgData.mData.mXformOpList[i];
            MObject mObject = sampPair.getObject();
            MFnTransform mFn(mObject, &status);
            unsigned int sampleSize = sampPair.sampledChannelSize();
            for (unsigned int j = 0; j < sampleSize; j ++)
            {

                srcPlug = srcArrayPlug.elementByLogicalIndex(logicalIndex++);

                std::string attrName = sampPair.getSampleElement(j);
                dstPlug = mFn.findPlug(attrName.c_str(), true);
                if (dstPlug.isNull())
                    continue;

                modifier.connect(srcPlug, dstPlug);
                status = modifier.doIt();
            }
        }
    }

    return alembicNodeFn.name();
}
