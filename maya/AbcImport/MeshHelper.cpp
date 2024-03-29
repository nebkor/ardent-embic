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

#include <maya/MString.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MUintArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MVector.h>
#include <maya/MDagModifier.h>

#include "util.h"
#include "MeshHelper.h"
#include "NodeIteratorVisitorHelper.h"


namespace
{

    MStatus setMeshUVs(MFnMesh & ioMesh,
        const MFloatArray & uArray, const MFloatArray & vArray,
        const MIntArray & uvCounts, const MIntArray & uvIds)
    {
        MStatus status = MS::kSuccess;

        // Create uv set on ioMesh object
        MString uvSetName("uvset1");
        status = ioMesh.getCurrentUVSetName(uvSetName);
        if ( status != MS::kSuccess )
        {
            uvSetName = MString("uvset1");
            status = ioMesh.createUVSet(uvSetName);
            status = ioMesh.setCurrentUVSetName(uvSetName);
        }
        status = ioMesh.clearUVs();
        status = ioMesh.setUVs(uArray, vArray, &uvSetName);
        status = ioMesh.assignUVs(uvCounts, uvIds);

        if (status != MS::kSuccess)
            printError(ioMesh.fullPathName() + " Assign UVs failed");

        return status;
    }  // setMeshUVs


    void setUVs(double iFrame, MFnMesh & ioMesh,
        Alembic::AbcGeom::IV2fGeomParam & iUVs)
    {

        if (!iUVs.valid())
            return;

        // no interpolation for now
        Alembic::AbcCoreAbstract::index_t index, ceilIndex;
        getWeightAndIndex(iFrame, iUVs.getTimeSampling(),
            iUVs.getNumSamples(), index, ceilIndex);

        MFloatArray uArray;
        MFloatArray vArray;

        unsigned int numFaceVertices = ioMesh.numFaceVertices();
        unsigned int numVertices = ioMesh.numVertices();

        MIntArray uvCounts(ioMesh.numPolygons(), 0);
        MIntArray uvIds(numFaceVertices, 0);

        Alembic::AbcGeom::IV2fGeomParam::Sample samp;
        iUVs.getIndexed(samp, Alembic::Abc::ISampleSelector(index));

        Alembic::AbcGeom::V2fArraySamplePtr uvPtr = samp.getVals();
        Alembic::Abc::UInt32ArraySamplePtr indexPtr = samp.getIndices();

        if (numFaceVertices != indexPtr->size() &&
            numVertices != indexPtr->size())
        {
            printWarning(
                ioMesh.fullPathName() +
                " UVs aren't per-vertex or per-polygon per-vertex, skipping");

            return;
        }

        std::size_t numUVs = uvPtr->size();
        uArray.setLength(numUVs);
        vArray.setLength(numUVs);
        for (std::size_t i = 0; i < numUVs; ++i)
        {
            uArray[i] = (*uvPtr)[i].x;
            vArray[i] = 1.0 - (*uvPtr)[i].y;
        }

        int uvIndex = 0;
        int uvCountsIndex = 0;

        int numPolys = ioMesh.numPolygons();

        // per-polygon per-vertex
        if (numFaceVertices == indexPtr->size())
        {
            for (int pIndex = 0; pIndex < numPolys; ++pIndex)
            {
                int numPolygonVertices = ioMesh.polygonVertexCount(pIndex);
                uvCounts[uvCountsIndex++] = numPolygonVertices;
                if (numPolygonVertices == 0)
                    continue;

                int startPoint = uvIndex + numPolygonVertices - 1;

                for (int vertexIndex = 0;
                    vertexIndex < numPolygonVertices; vertexIndex++)
                {
                    uvIds[uvIndex++] = (*indexPtr)[startPoint - vertexIndex];
                }
            }
        }
        // per-vertex
        else
        {
            MIntArray vertexCount, vertexList;
            ioMesh.getVertices(vertexCount, vertexList);
            for (int pIndex = 0; pIndex < numPolys; ++pIndex)
            {
                int numPolygonVertices = ioMesh.polygonVertexCount(pIndex);
                uvCounts[uvCountsIndex++] = numPolygonVertices;
                if (numPolygonVertices == 0)
                    continue;

                int startPoint = uvIndex + numPolygonVertices - 1;

                for (int vertexIndex = 0;
                    vertexIndex < numPolygonVertices; vertexIndex++)
                {
                    uvIds[uvIndex++] = (*indexPtr)[
                        vertexList[startPoint - vertexIndex]];
                }
            }
        }

        setMeshUVs(ioMesh, uArray, vArray, uvCounts, uvIds);
    }  // setUVs

    // utility to clear pt when doing a swap otherwise
    // the new swap position could get messed up
    void clearPt(MFnMesh & ioMesh)
    {
        MPlug ptPlug = ioMesh.findPlug("pt");
        unsigned int numElements = ptPlug.numElements();
        if (ptPlug.isArray() && (numElements > 0))
        {
            for (unsigned int i = 0; i < numElements; i++)
            {
                MPlug elementPlug = ptPlug[i];
                MPlug childx = elementPlug.child(0);
                childx.setValue(0.0);
                MPlug childy = elementPlug.child(1);
                childy.setValue(0.0);
                MPlug childz = elementPlug.child(2);
                childz.setValue(0.0);
            }
        }
    }

    // normal vector is packed differently in file
    // from the format Maya accepts directly
    void setPolyNormals(double iFrame, MFnMesh & ioMesh,
        Alembic::AbcGeom::IN3fGeomParam iNormals)
    {
        // no normals to set?  bail early
        if (!iNormals)
            return;

        if (iNormals.getScope() != Alembic::AbcGeom::kVertexScope &&
            iNormals.getScope() != Alembic::AbcGeom::kFacevaryingScope)
        {
            printWarning(ioMesh.fullPathName() +
                " normal vector has an unsupported scope, skipping normals");
            return;
        }

        Alembic::AbcCoreAbstract::index_t index, ceilIndex;
        double alpha = getWeightAndIndex(iFrame,
            iNormals.getTimeSampling(), iNormals.getNumSamples(),
            index, ceilIndex);

        Alembic::AbcGeom::IN3fGeomParam::Sample samp;
        iNormals.getExpanded(samp, Alembic::Abc::ISampleSelector(index));

        MVectorArray normalsIn;

        Alembic::Abc::N3fArraySamplePtr sampVal = samp.getVals();
        size_t sampSize = sampVal->size();

        Alembic::Abc::N3fArraySamplePtr ceilVals;
        if (alpha != 0 && index != ceilIndex)
        {
            Alembic::AbcGeom::IN3fGeomParam::Sample ceilSamp;
            iNormals.getExpanded(ceilSamp,
                Alembic::Abc::ISampleSelector(ceilIndex));
            ceilVals = ceilSamp.getVals();
            if (sampSize == ceilVals->size())
            {
                Alembic::Abc::N3fArraySamplePtr ceilVal = ceilSamp.getVals();
                for (size_t i = 0; i < sampSize; ++i)
                {
                    MVector normal(
                        simpleLerp<float>(alpha, (*sampVal)[i].x,
                            (*ceilVal)[i].x),
                        simpleLerp<float>(alpha, (*sampVal)[i].y,
                            (*ceilVal)[i].y),
                        simpleLerp<float>(alpha, (*sampVal)[i].z,
                            (*ceilVal)[i].z));
                    normalsIn.append(normal);
                }
            }
            else
            {
                for (size_t i = 0; i < sampSize; ++i)
                {
                    MVector normal((*sampVal)[i].x, (*sampVal)[i].y,
                        (*sampVal)[i].z);
                    normalsIn.append(normal);
                }
            }
        }
        else
        {
            for (size_t i = 0; i < sampSize; ++i)
            {
                MVector normal((*sampVal)[i].x, (*sampVal)[i].y,
                    (*sampVal)[i].z);
                normalsIn.append(normal);
            }
        }

        if (iNormals.getScope() == Alembic::AbcGeom::kVertexScope &&
            sampSize == ( std::size_t ) ioMesh.numVertices())
        {
            MIntArray vertexList;
            int iEnd = static_cast<int>(sampSize);
            for (int i = 0; i < iEnd; ++i)
            {
                vertexList.append(i);
            }

            ioMesh.setVertexNormals(normalsIn, vertexList);

        }
        else if (sampSize == ( std::size_t ) ioMesh.numFaceVertices() &&
            iNormals.getScope() == Alembic::AbcGeom::kFacevaryingScope)
        {

            MIntArray faceList(static_cast<unsigned int>(sampSize));
            MIntArray vertexList(static_cast<unsigned int>(sampSize));

            // per vertex per-polygon normal
            int numFaces = ioMesh.numPolygons();
            int nIndex = 0;
            for (int faceIndex = 0; faceIndex < numFaces; faceIndex++)
            {
                MIntArray polyVerts;
                ioMesh.getPolygonVertices(faceIndex, polyVerts);
                int numVertices = polyVerts.length();
                for (int v = numVertices - 1; v >= 0; v--, ++nIndex)
                {
                    faceList[nIndex] = faceIndex;
                    vertexList[nIndex] = polyVerts[v];
                }
            }

            ioMesh.setFaceVertexNormals(normalsIn, faceList, vertexList);
        }
        else
        {
            printWarning(ioMesh.fullPathName() +
                " normal vector scope does not match size of data, " +
                "skipping normals");
        }
    }

    void fillPoints(MFloatPointArray & oPointArray,
        Alembic::Abc::P3fArraySamplePtr iPoints,
        Alembic::Abc::P3fArraySamplePtr iCeilPoints, double alpha)
    {
        unsigned int numPoints = static_cast<unsigned int>(iPoints->size());
        oPointArray.setLength(numPoints);

        if (alpha == 0 || iCeilPoints == NULL)
        {
            for (unsigned int i = 0; i < numPoints; ++i)
            {
                oPointArray.set(i,
                    (*iPoints)[i].x, (*iPoints)[i].y, (*iPoints)[i].z);
            }
        }
        else
        {
            for (unsigned int i = 0; i < numPoints; ++i)
            {
                oPointArray.set(i,
                    simpleLerp<float>(alpha,
                        (*iPoints)[i].x, (*iCeilPoints)[i].x),
                    simpleLerp<float>(alpha,
                        (*iPoints)[i].y, (*iCeilPoints)[i].y),
                    simpleLerp<float>(alpha,
                        (*iPoints)[i].z, (*iCeilPoints)[i].z));
            }
        }

    }

    void fillTopology(MFnMesh & ioMesh, MObject & iParent,
        MFloatPointArray & iPoints,
        Alembic::Abc::Int32ArraySamplePtr iIndices,
        Alembic::Abc::Int32ArraySamplePtr iCounts)
    {
        // since we are changing the topology we will be creating a new mesh

        // Get face count info
        unsigned int numPolys = static_cast<unsigned int>(iCounts->size());
        MIntArray polyCounts;
        polyCounts.setLength(numPolys);

        for (unsigned int i = 0; i < numPolys; ++i)
        {
            polyCounts[i] = (*iCounts)[i];
        }

        unsigned int numConnects = static_cast<unsigned int>(iIndices->size());

        MIntArray polyConnects;
        polyConnects.setLength(numConnects);

        unsigned int facePointIndex = 0;
        unsigned int base = 0;
        for (unsigned int i = 0; i < numPolys; ++i)
        {
            // reverse the order of the faces
            int curNum = polyCounts[i];
            for (int j = 0; j < curNum; ++j, ++facePointIndex)
                polyConnects[facePointIndex] = (*iIndices)[base+curNum-j-1];

            base += curNum;
        }

        MObject shape = ioMesh.create(iPoints.length(), numPolys, iPoints,
            polyCounts, polyConnects, iParent);

    }

}  // namespace

// once normals are supported in the polyMesh schema, polyMesh will look
// different than readSubD
void readPoly(double iFrame, MFnMesh & ioMesh, MObject & iParent,
    Alembic::AbcGeom::IPolyMesh & iNode, bool iInitialized)
{
    Alembic::AbcGeom::IPolyMeshSchema schema = iNode.getSchema();
    Alembic::AbcGeom::MeshTopologyVariance ttype = schema.getTopologyVariance();

    Alembic::AbcCoreAbstract::index_t index, ceilIndex;
    double alpha = getWeightAndIndex(iFrame,
        schema.getTimeSampling(), schema.getNumSamples(), index, ceilIndex);

    MFloatPointArray pointArray;
    Alembic::Abc::P3fArraySamplePtr ceilPoints;

    // we can just read the points
    if (ttype != Alembic::AbcGeom::kHeterogenousTopology && iInitialized)
    {

        Alembic::Abc::P3fArraySamplePtr points = schema.getPositionsProperty(
            ).getValue(Alembic::Abc::ISampleSelector(index));

        if (alpha != 0.0)
        {
            ceilPoints = schema.getPositionsProperty().getValue(
                Alembic::Abc::ISampleSelector(ceilIndex) );
        }

        fillPoints(pointArray, points, ceilPoints, alpha);
        ioMesh.setPoints(pointArray, MSpace::kObject);

        if (schema.getNormalsParam().getNumSamples() > 1)
        {
            setPolyNormals(iFrame, ioMesh, schema.getNormalsParam());
        }

        if (schema.getUVsParam().getNumSamples() > 1)
        {
            setUVs(iFrame, ioMesh, schema.getUVsParam());
        }

        return;
    }

    // we need to read the topology
    Alembic::AbcGeom::IPolyMeshSchema::Sample samp;
    schema.get(samp, Alembic::Abc::ISampleSelector(index));

    if (alpha != 0.0 && ttype != Alembic::AbcGeom::kHeterogenousTopology)
    {
        ceilPoints = schema.getPositionsProperty().getValue(
            Alembic::Abc::ISampleSelector(ceilIndex) );
    }

    fillPoints(pointArray, samp.getPositions(), ceilPoints, alpha);

    fillTopology(ioMesh, iParent, pointArray, samp.getFaceIndices(),
        samp.getFaceCounts());

    setPolyNormals(iFrame, ioMesh, schema.getNormalsParam());
    setUVs(iFrame, ioMesh, schema.getUVsParam());
}

void readSubD(double iFrame, MFnMesh & ioMesh, MObject & iParent,
    Alembic::AbcGeom::ISubD & iNode, bool iInitialized)
{
    Alembic::AbcGeom::ISubDSchema schema = iNode.getSchema();
    Alembic::AbcGeom::MeshTopologyVariance ttype = schema.getTopologyVariance();

    Alembic::AbcCoreAbstract::index_t index, ceilIndex;
    double alpha = getWeightAndIndex(iFrame,
        schema.getTimeSampling(), schema.getNumSamples(), index, ceilIndex);

    MFloatPointArray pointArray;
    Alembic::Abc::P3fArraySamplePtr ceilPoints;

    // we can just read the points
    if (ttype != Alembic::AbcGeom::kHeterogenousTopology && iInitialized)
    {

        Alembic::Abc::P3fArraySamplePtr points = schema.getPositionsProperty(
            ).getValue(Alembic::Abc::ISampleSelector(index));

        if (alpha != 0.0)
        {
            ceilPoints = schema.getPositionsProperty().getValue(
                Alembic::Abc::ISampleSelector(ceilIndex) );
        }

        fillPoints(pointArray, points, ceilPoints, alpha);
        ioMesh.setPoints(pointArray, MSpace::kObject);

        if (schema.getUVsParam().getNumSamples() > 1)
        {
            setUVs(iFrame, ioMesh, schema.getUVsParam());
        }

        return;
    }

    // we need to read the topology
    Alembic::AbcGeom::ISubDSchema::Sample samp;
    schema.get(samp, Alembic::Abc::ISampleSelector(index));

    if (alpha != 0.0 && ttype != Alembic::AbcGeom::kHeterogenousTopology)
    {
        ceilPoints = schema.getPositionsProperty().getValue(
            Alembic::Abc::ISampleSelector(ceilIndex) );
    }

    fillPoints(pointArray, samp.getPositions(), ceilPoints, alpha);

    fillTopology(ioMesh, iParent, pointArray, samp.getFaceIndices(),
        samp.getFaceCounts());

    setUVs(iFrame, ioMesh, schema.getUVsParam());
}

void disconnectMesh(MObject & iMeshObject,
    std::vector<Prop> & iSampledPropList,
    std::size_t iFirstProp)
{
    MFnMesh fnMesh;
    fnMesh.setObject(iMeshObject);

    // disconnect old connection from AlembicNode or some other nodes
    // to inMesh if one such connection exist
    MPlug dstPlug = fnMesh.findPlug("inMesh");
    disconnectAllPlugsTo(dstPlug);

    disconnectProps(fnMesh, iSampledPropList, iFirstProp);

    clearPt(fnMesh);

    return;

}

MObject createPoly(double iFrame, Alembic::AbcGeom::IPolyMesh & iNode,
    MObject & iParent)
{
    Alembic::AbcGeom::IPolyMeshSchema schema = iNode.getSchema();
    MString name(iNode.getName().c_str());

    MStatus status = MS::kSuccess;

    MFnMesh fnMesh;
    MObject obj;

    // add other properties
    if (schema.getNumSamples() > 1)
    {
        MFloatPointArray emptyPt;
        MIntArray emptyInt;
        obj = fnMesh.create(0, 0, emptyPt, emptyInt, emptyInt, iParent);
        fnMesh.setName(name);
    }
    else
    {
        Alembic::AbcCoreAbstract::index_t index, ceilIndex;
        double alpha = getWeightAndIndex(iFrame, schema.getTimeSampling(),
            schema.getNumSamples(), index, ceilIndex);

        Alembic::AbcGeom::IPolyMeshSchema::Sample samp;
        schema.get(samp, Alembic::Abc::ISampleSelector(index));

        MFloatPointArray ptArray;
        Alembic::Abc::P3fArraySamplePtr ceilPoints;
        if (index != ceilIndex)
        {
            Alembic::AbcGeom::IPolyMeshSchema::Sample ceilSamp;
            schema.get(ceilSamp, Alembic::Abc::ISampleSelector(ceilIndex));
            ceilPoints = ceilSamp.getPositions();
        }

        fillPoints(ptArray, samp.getPositions(), ceilPoints, alpha);
        fillTopology(fnMesh, iParent, ptArray, samp.getFaceIndices(),
            samp.getFaceCounts());
        fnMesh.setName(iNode.getName().c_str());
        setPolyNormals(iFrame, fnMesh, schema.getNormalsParam());
        setUVs(iFrame, fnMesh, schema.getUVsParam());
        obj = fnMesh.object();
    }

    MString pathName = fnMesh.partialPathName();
    setInitialShadingGroup(pathName);

    if ( !schema.getNormalsParam().valid() )
    {
        MFnNumericAttribute attr;
        MString attrName("noNormals");
        MObject attrObj = attr.create(attrName, attrName,
        MFnNumericData::kBoolean, true, &status);
        attr.setKeyable(true);
        attr.setHidden(false);
        MFnMesh fnMesh(obj);
        fnMesh.addAttribute(attrObj, MFnDependencyNode::kLocalDynamicAttr);
    }

    return obj;
}

MObject createSubD(double iFrame, Alembic::AbcGeom::ISubD & iNode,
    MObject & iParent)
{
    Alembic::AbcGeom::ISubDSchema schema = iNode.getSchema();

    Alembic::AbcCoreAbstract::index_t index, ceilIndex;
    getWeightAndIndex(iFrame, schema.getTimeSampling(),
        schema.getNumSamples(), index, ceilIndex);

    Alembic::AbcGeom::ISubDSchema::Sample samp;
    schema.get(samp, Alembic::Abc::ISampleSelector(index));

    MString name(iNode.getName().c_str());

    MFnMesh fnMesh;

    MFloatPointArray pointArray;
    Alembic::Abc::P3fArraySamplePtr emptyPtr;
    fillPoints(pointArray, samp.getPositions(), emptyPtr, 0.0);

    fillTopology(fnMesh, iParent, pointArray, samp.getFaceIndices(),
        samp.getFaceCounts());
    fnMesh.setName(iNode.getName().c_str());

    setInitialShadingGroup(fnMesh.partialPathName());

    MObject obj = fnMesh.object();

    setUVs(iFrame, fnMesh, schema.getUVsParam());

    // add the mFn-specific attributes to fnMesh node
    MFnNumericAttribute numAttr;
    MString attrName("SubDivisionMesh");
    MObject attrObj = numAttr.create(attrName, attrName,
        MFnNumericData::kBoolean, 1);
    numAttr.setKeyable(true);
    numAttr.setHidden(false);
    fnMesh.addAttribute(attrObj, MFnDependencyNode::kLocalDynamicAttr);

    if (samp.getInterpolateBoundary() > 0)
    {
        attrName = MString("interpolateBoundary");
        attrObj = numAttr.create(attrName, attrName, MFnNumericData::kBoolean,
            samp.getInterpolateBoundary());

        numAttr.setKeyable(true);
        numAttr.setHidden(false);
        fnMesh.addAttribute(attrObj,  MFnDependencyNode::kLocalDynamicAttr);
    }

    if (samp.getFaceVaryingInterpolateBoundary() > 0)
    {
        attrName = MString("faceVaryingInterpolateBoundary");
        attrObj = numAttr.create(attrName, attrName, MFnNumericData::kBoolean,
            samp.getFaceVaryingInterpolateBoundary());

        numAttr.setKeyable(true);
        numAttr.setHidden(false);
        fnMesh.addAttribute(attrObj,  MFnDependencyNode::kLocalDynamicAttr);
    }

    if (samp.getFaceVaryingPropagateCorners() > 0)
    {
        attrName = MString("faceVaryingPropagateCorners");
        attrObj = numAttr.create(attrName, attrName, MFnNumericData::kBoolean,
            samp.getFaceVaryingPropagateCorners());

        numAttr.setKeyable(true);
        numAttr.setHidden(false);
        fnMesh.addAttribute(attrObj,  MFnDependencyNode::kLocalDynamicAttr);
    }

#if MAYA_API_VERSION >= 201100
    Alembic::Abc::Int32ArraySamplePtr holes = samp.getHoles();
    if (holes && !holes->size() == 0)
    {
        std::size_t numHoles = holes->size();
        MUintArray holeData(numHoles);
        for (std::size_t i = 0; i < numHoles; ++i)
        {
            holeData[i] = (*holes)[i];
        }

        if (fnMesh.setInvisibleFaces(holeData) != MS::kSuccess)
        {
            MString warn = "Failed to set holes on: ";
            warn += iNode.getName().c_str();
            printWarning(warn);
        }
    }
#endif

    Alembic::Abc::FloatArraySamplePtr creases = samp.getCreaseSharpnesses();
    if (creases && !creases->size() == 0)
    {
        Alembic::Abc::Int32ArraySamplePtr indices = samp.getCreaseIndices();
        Alembic::Abc::Int32ArraySamplePtr lengths = samp.getCreaseLengths();
        std::size_t numLengths = lengths->size();

        MUintArray edgeIds;
        MDoubleArray creaseData;

        std::size_t curIndex = 0;
        // curIndex incremented here to move on to the next crease length
        for (std::size_t i = 0; i < numLengths; ++i, ++curIndex)
        {
            std::size_t len = (*lengths)[i] - 1;
            float creaseSharpness = (*creases)[i];

            // curIndex incremented here to go between all the edges that make
            // up a given length
            for (std::size_t j = 0; j < len; ++j, ++curIndex)
            {
                Alembic::Util::int32_t vertA = (*indices)[curIndex];
                Alembic::Util::int32_t vertB = (*indices)[curIndex+1];
                MItMeshVertex itv(obj);

                int prev;
                itv.setIndex(vertA, prev);

                MIntArray edges;
                itv.getConnectedEdges(edges);
                std::size_t numEdges = edges.length();
                for (std::size_t k = 0; k < numEdges; ++k)
                {
                    int oppVert = -1;
                    itv.getOppositeVertex(oppVert, edges[k]);
                    if (oppVert == vertB)
                    {
                        creaseData.append(creaseSharpness);
                        edgeIds.append(edges[k]);
                        break;
                    }
                }
            }
        }
        if (fnMesh.setCreaseEdges(edgeIds, creaseData) != MS::kSuccess)
        {
            MString warn = "Failed to set creases on: ";
            warn += iNode.getName().c_str();
            printWarning(warn);
        }
    }

    Alembic::Abc::FloatArraySamplePtr corners = samp.getCornerSharpnesses();
    if (corners && !corners->size() == 0)
    {
        Alembic::Abc::Int32ArraySamplePtr cornerVerts = samp.getCornerIndices();
        unsigned int numCorners = static_cast<unsigned int>(corners->size());
        MUintArray vertIds(numCorners);
        MDoubleArray cornerData(numCorners);

        for (unsigned int i = 0; i < numCorners; ++i)
        {
            cornerData[i] = (*corners)[i];
            vertIds[i] = (*cornerVerts)[i];
        }
        if (fnMesh.setCreaseVertices(vertIds, cornerData) != MS::kSuccess)
        {
            MString warn = "Failed to set corners on: ";
            warn += iNode.getName().c_str();
            printWarning(warn);
        }
    }

    return obj;
}
