//-*****************************************************************************
//
// Copyright (c) 2009-2011,
//  Sony Pictures Imageworks Inc. and
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
// Industrial Light & Magic, nor the names of their contributors may be used
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

#include "MayaUtility.h"

// return seconds per frame
double util::spf()
{
    static const MTime sec(1.0, MTime::kSeconds);
    return 1.0 / sec.as(MTime::uiUnit());
}

bool util::isAncestorDescendentRelationship(const MDagPath & path1,
    const MDagPath & path2)
{
    unsigned int length1 = path1.length();
    unsigned int length2 = path2.length();
    unsigned int diff;

    if (length1 == length2 && !(path1 == path2))
        return false;

    MDagPath ancestor, descendent;
    if (length1 > length2)
    {
        ancestor = path2;
        descendent = path1;
        diff = length1 - length2;
    }
    else
    {
        ancestor = path1;
        descendent = path2;
        diff = length2 - length1;
    }

    descendent.pop(diff);

    bool ret = (ancestor == descendent);

    if (ret)
    {
        MString err = path1.fullPathName() + " and ";
        err += path2.fullPathName() + " have parenting relationships";
        MGlobal::displayError(err);
    }
    return ret;
}



// returns 0 if static, 1 if sampled, and 2 if a curve
int util::getSampledType(const MPlug& iPlug)
{
    MPlugArray conns;

    iPlug.connectedTo(conns, true, false);

    // it's possible that only some element of an array plug or
    // some component of a compound plus is connected
    if (conns.length() == 0)
    {
        if (iPlug.isArray())
        {
            unsigned int numConnectedElements = iPlug.numConnectedElements();
            for (unsigned int e = 0; e < numConnectedElements; e++)
            {
                int retVal = getSampledType(iPlug.connectionByPhysicalIndex(e));
                if (retVal > 0)
                    return retVal;
            }
        }
        else if (iPlug.isCompound() && iPlug.numConnectedChildren() > 0)
        {
            unsigned int numChildren = iPlug.numChildren();
            for (unsigned int c = 0; c < numChildren; c++)
            {
                int retVal = getSampledType(iPlug.child(c));
                if (retVal > 0)
                    return retVal;
            }
        }
        return 0;
    }

    MObject ob;
    MFnDependencyNode nodeFn;
    for (unsigned i = 0; i < conns.length(); i++)
    {
        ob = conns[i].node();
        MFn::Type type = ob.apiType();

        switch (type)
        {
            case MFn::kAnimCurveTimeToAngular:
            case MFn::kAnimCurveTimeToDistance:
            case MFn::kAnimCurveTimeToTime:
            case MFn::kAnimCurveTimeToUnitless:
            {
                nodeFn.setObject(ob);
                MPlug incoming = nodeFn.findPlug("i", true);

                // sampled
                if (incoming.isConnected())
                    return 1;

                // curve
                else
                    return 2;
            }
            break;

            case MFn::kMute:
            {
                nodeFn.setObject(ob);
                MPlug mutePlug = nodeFn.findPlug("mute", true);

                // static
                if (mutePlug.asBool())
                    return 0;
                // curve
                else
                   return 2;
            }
            break;

            default:
            break;
        }
    }

    return 1;
}

bool util::getRotOrder(MTransformationMatrix::RotationOrder iOrder,
    unsigned int & oXAxis, unsigned int & oYAxis, unsigned int & oZAxis)
{
    switch (iOrder)
    {
        case MTransformationMatrix::kXYZ:
        {
            oXAxis = 0;
            oYAxis = 1;
            oZAxis = 2;
        }
        break;

        case MTransformationMatrix::kYZX:
        {
            oXAxis = 1;
            oYAxis = 2;
            oZAxis = 0;
        }
        break;

        case MTransformationMatrix::kZXY:
        {
            oXAxis = 2;
            oYAxis = 0;
            oZAxis = 1;
        }
        break;

        case MTransformationMatrix::kXZY:
        {
            oXAxis = 0;
            oYAxis = 2;
            oZAxis = 1;
        }
        break;

        case MTransformationMatrix::kYXZ:
        {
            oXAxis = 1;
            oYAxis = 0;
            oZAxis = 2;
        }
        break;

        case MTransformationMatrix::kZYX:
        {
            oXAxis = 2;
            oYAxis = 1;
            oZAxis = 0;
        }
        break;

        default:
        {
            return false;
        }
    }
    return true;
}

// 0 dont write, 1 write static 0, 2 write anim 0, 3 write anim -1
int util::getVisibilityType(const MPlug & iPlug)
{
    int type = getSampledType(iPlug);

    // static case
    if (type == 0)
    {
        // dont write anything
        if (iPlug.asBool())
            return 0;

        // write static 0
        return 1;
    }
    else
    {
        // anim write -1
        if (iPlug.asBool())
            return 3;

        // write anim 0
        return 2;
    }
}

// does this cover all cases?
bool util::isAnimated(MObject & object, bool checkParent)
{
    MStatus stat;
    MItDependencyGraph iter(object, MFn::kInvalid,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kPlugLevel,
        &stat);

    if (stat!= MS::kSuccess)
    {
        MGlobal::displayError("Unable to create DG iterator ");
    }

    for (; !iter.isDone(); iter.next())
    {
        MObject node = iter.thisNode();
        MPlug plug = iter.thisPlug();

        if (node.hasFn(MFn::kExpression))
        {
            MFnExpression fn(node, &stat);
            if (stat == MS::kSuccess && fn.isAnimated())
            {
                return true;
            }
        }
        if (MAnimUtil::isAnimated(node, checkParent))
        {
            return true;
        }
        if (node.hasFn(MFn::kPluginDependNode) ||
                node.hasFn( MFn::kConstraint ) ||
                node.hasFn(MFn::kPointConstraint) ||
                node.hasFn(MFn::kAimConstraint) ||
                node.hasFn(MFn::kOrientConstraint) ||
                node.hasFn(MFn::kScaleConstraint) ||
                node.hasFn(MFn::kGeometryConstraint) ||
                node.hasFn(MFn::kNormalConstraint) ||
                node.hasFn(MFn::kTangentConstraint) ||
                node.hasFn(MFn::kParentConstraint) ||
                node.hasFn(MFn::kPoleVectorConstraint) ||
                node.hasFn(MFn::kParentConstraint) ||
                node.hasFn(MFn::kTime) ||
                node.hasFn(MFn::kJoint) ||
                node.hasFn(MFn::kGeometryFilt) ||
                node.hasFn(MFn::kTweak) ||
                node.hasFn(MFn::kPolyTweak) ||
                node.hasFn(MFn::kSubdTweak) ||
                node.hasFn(MFn::kCluster) ||
                node.hasFn(MFn::kFluid))
        {
            return true;
        }
    }

    return false;
}

bool util::isIntermediate(const MObject & object)
{
    MStatus stat;
    MFnDagNode mFn(object);

    MPlug plug = mFn.findPlug("intermediateObject", false, &stat);
    if (stat == MS::kSuccess && plug.asBool())
        return true;
    else
        return false;
}

bool util::isRenderable(const MObject & object)
{
    MStatus stat;
    MFnDagNode mFn(object);

    // templated turned on?  return false
    MPlug plug = mFn.findPlug("template", false, &stat);
    if (stat == MS::kSuccess && plug.asBool())
        return false;

    // visibility or lodVisibility off?  return false
    plug = mFn.findPlug("visibility", false, &stat);
    if (stat == MS::kSuccess && !plug.asBool())
        return false;

    plug = mFn.findPlug("lodVisibility", false, &stat);
    if (stat == MS::kSuccess && !plug.asBool())
        return false;

    // this shape is renderable
    return true;
}

MString util::stripNamespaces(const MString & iNodeName)
{
    int lastNamespace = iNodeName.rindex(':');
    if (lastNamespace != -1)
    {
        return iNodeName.substring(lastNamespace + 1, iNodeName.length() - 1);
    }

    return iNodeName;
}

MString util::getHelpText()
{
    MString ret =
"AbcExport [options]\n"
"Options:\n"
"-h / -help  Print this message.\n"
"\n"
"-prs / -preRollStartFrame double\n"
"The frame to start scene evaluation at.  This is used to set the\n"
"starting frame for time dependent translations and can be used to evaluate\n"
"run-up that isn't actually translated.\n"
"\n"
"-duf / -dontSkipUnwrittenFrames\n"
"When evaluating multiple translate jobs, the presence of this flag decides\n"
"whether to evaluate frames between jobs when there is a gap in their frame\n"
"ranges.\n"
"\n"
"-v / -verbose\n"
"Prints the current frame that is being evaluated.\n"
"\n"
"-j / -jobArg string REQUIRED\n"
"String which contains flags for writing data to a particular file.\n"
"Multiple jobArgs can be specified.\n"
"\n"
"-jobArg flags:\n"
"\n"
"-a / -attr string\n"
"A specific attribute to write out.  This flag may occur more than once.\n"
"\n"
"-atp / -attrPrefix string (default ABC_)\n"
"Prefix filter for determining which attributes to write out.\n"
"This flag may occur more than once.\n"
"\n"
"-f / -file string REQUIRED\n"
"File location to write the Alembic data.\n"
"\n"
"-fr / -frameRange double double\n"
"The frame range to write.\n"
"\n"
"-frs / -frameRelativeSample double\n"
"frame relative sample that will be written out along the frame range.\n"
"This flag may occur more than once.\n"
"\n"
"-nn / -noNormals\n"
"If this flag is present normal data for Alembic poly meshes will not be\n"
"written.\n"
"\n"
"-ro / -renderableOnly\n"
"If this flag is present non-renderable hierarchy (invisible, or templated)\n"
"will not be written out.\n"
"\n"
"-rt / -root\n"
"Maya dag path which will be parented to the root of the Alembic file.\n"
"This flag may occur more than once.  If unspecified, it defaults to '|' which\n"
"means the entire scene will be written out.\n"
"\n"
"-s / -step double (default 1.0)\n"
"The step time between each sample to write out the data.\n"
"\n"
"-sl / -selection\n"
"If this flag is present, write out all all selected nodes from the active\n"
"selection list that are descendents of the roots specified with -root.\n"
"\n"
"-sn / -stripNamespaces\n"
"If this flag is present all namespaces will be stripped off of the node before\n"
"being written to Alembic.  Be careful that the new stripped name does not\n"
"collide with other sibling node names.\n"
"Example:  taco:foo:bar would be written as just bar.\n"
"\n"
"-uv / -uvWrite\n"
"If this flag is present, uv data for PolyMesh and SubD shapes will be written to\n"
"the Alembic file.  Only the current uv map is used.\n"
"\n"
"-wfg / -wholeFrameGeo\n"
"If this flag is present data for geometry will only be written out on whole\n"
"\nframes."
"\n"
"-ws / -worldSpace\n"
"If this flag is present, any root nodes will be stored in world space.\n"
"\n"
"-wv / -writeVisibility\n"
"If this flag is present, visibility state will be stored in the Alembic\n"
"file.  Otherwise everything written out is treated as visible.\n"
"\n"
"-mfc / -melPerFrameCallback string\n"
"When each frame (and the static frame) is evaluated the string specified is\n"
"evaluated as a Mel command. See below for special processing rules.\n"
"\n"
"-mpc / -melPostJobCallback string\n"
"When the translation has finished the string specified is evaluated as a Mel\n"
"command. See below for special processing rules.\n"
"\n"
"-pfc / -pythonPerFrameCallback string\n"
"When each frame (and the static frame) is evaluated the string specified is\n"
"evaluated as a python command. See below for special processing rules.\n"
"\n"
"-ppc / -pythonPostJobCallback string\n"
"When the translation has finished the string specified is evaluated as a\n"
"python command. See below for special processing rules.\n"
"\n"
"Special callback information:\n"
"On the callbacks, special tokens are replaced with other data, these tokens\n"
"and what they are replaced with are as follows:\n"
"\n"
"#FRAME# replaced with the frame number being evaluated.\n"
"#FRAME# is ignored in the post callbacks.\n"
"\n"
"#BOUNDS# replaced with a string holding bounding box values in minX minY minZ\n"
"maxX maxY maxZ space seperated order.\n"
"\n"
"#BOUNDSARRAY# replaced with the bounding box values as above, but in\n"
"array form.\n"
"In Mel: {minX, minY, minZ, maxX, maxY, maxZ}\n"
"In Python: [minX, minY, minZ, maxX, maxY, maxZ]\n"
"\n"
"Examples:\n"
"\n"
"AbcExport -j \"-root |group|foo -root |test|path|bar -file /tmp/test.abc\"\n"
"Writes out everything at foo and below and bar and below to /tmp/test.abc.\n"
"foo and bar are siblings parented to the root of the Alembic scene.\n"
"\n"
"AbcExport -j \"-frameRange 1 5 -step 0.5 -root |group|foo -file /tmp/test.abc\"\n"
"Writes out everything at foo and below to /tmp/test.abc sampling at frames:\n"
"1 1.5 2 2.5 3 3.5 4 4.5 5\n"
"\n"
"AbcExport -j \"-fr 0 10 -frs -0.1 -frs 0.2 -step 5 -file /tmp/test.abc\"\n"
"Writes out everything in the scene to /tmp/test.abc sampling at frames:\n"
"-0.1 0.2 4.9 5.2 9.9 10.2\n"
"\n"
"Note: The difference between your highest and lowest frameRelativeSample can\n"
"not be greater than your step size.\n"
"\n"
"AbcExport -j \"-step 0.25 -frs 0.3 -frs 0.60 -fr 1 5 -root foo -file test.abc\"\n"
"\n"
"Is illegal because the higest and lowerst frameRelativeSamples are 0.3 frames\n"
"apart.\n"
"\n"
"AbcExport -j \"-sel -root |group|foo -file /tmp/test.abc\"\n"
"Writes out all selected nodes and it's ancestor nodes including up to foo.\n"
"foo will be parented to the root of the Alembic scene.\n"
"\n";

    return ret;
}
