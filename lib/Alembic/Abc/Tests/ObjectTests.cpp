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

#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/Abc/All.h>
#include <boost/random.hpp>
#include <boost/lexical_cast.hpp>

namespace Abc = Alembic::Abc;
using namespace Abc;

//
// The tests in this file are intended to exercize the Abc
//  API; specifically writing and reading of simple objects in
//  parent-child hierarchies.
//

void writeFlatHierarchy(const std::string &archiveName)
{
    const unsigned int numChildren = 10;

    // Create an archive for writing. Indicate that we want Alembic to
    //   throw exceptions on errors.
    OArchive archive( Alembic::AbcCoreHDF5::WriteArchive(),
                      archiveName, ErrorHandler::kThrowPolicy );
    OObject archiveTop = archive.getTop();

    for (int ii=0; ii<numChildren; ii++)
    {
        // Create 'numChildren' children, all parented under
        //  the archive
        std::string name = "child_";
        name.append( boost::lexical_cast<std::string>( ii ) );
        OObject child( archiveTop, name );

        // No properties.
    }

    // Done - the archive closes itself
}


void readFlatHierarchy(const std::string &archiveName)
{
    // Open an existing archive for reading. Indicate that we want
    //   Alembic to throw exceptions on errors.
    IArchive archive( Alembic::AbcCoreHDF5::ReadArchive(),
                      archiveName, ErrorHandler::kThrowPolicy );
    IObject archiveTop = archive.getTop();

    // Determine the number of (top level) children the archive has
    const unsigned int numChildren = archiveTop.getNumChildren();
    ABCA_ASSERT( numChildren == 10,
                 "Expected 10 children, found " << numChildren );

    std::cout << "The archive has " << numChildren << " children:"
              << std::endl;

    // Iterate through them, print out their names
    for (int ii=0; ii<numChildren; ii++)
    {
        IObject child( archiveTop,
                       archiveTop.getChildHeader(ii).getName() );
        std::cout << "  " << child.getName();

        const unsigned int children = child.getNumChildren();
        std::cout << " has " << children << " children"
                  << std::endl;
        
        ABCA_ASSERT( children == 0,
                     "Expected no children, found " << children );

    }

    // Done - the archive closes itself
}

/////////////////////

void recursivelyAddChildren( OObject&           parent,
                             const unsigned int depth,
                             unsigned int &     d,
                             const unsigned int numChildren )
{
    if (d == depth)
        return;

    d++;
    for (unsigned int ii=0; ii<numChildren; ii++)
    {
        std::string name = "child_";
        name.append( boost::lexical_cast<std::string>( d ) );
        name += "_";
        name.append( boost::lexical_cast<std::string>( ii ) );
        OObject child( parent, name );

        unsigned int dd = d;
        recursivelyAddChildren( child, depth, d, numChildren);
        d = dd;
    }

    return;
}


void writeThreeDeepHierarchy(const std::string &archiveName)
{
    const unsigned int numChildren = 2;
    const unsigned int depth = 2; // 1 level at the top

    // Create an archive for writing. Indicate that we want Alembic to
    //   throw exceptions on errors.
    OArchive archive( Alembic::AbcCoreHDF5::WriteArchive(),
                      archiveName, ErrorHandler::kThrowPolicy );
    OObject archiveTop = archive.getTop();

    // Add children to the top ('archive') level, and then recurse
    //  on these children, adding more as we go
    for (unsigned int ii=0; ii<numChildren; ii++)
    {
        unsigned int d = 0;
        std::string name = "child_0_";
        name.append( boost::lexical_cast<std::string>( ii ) );
        OObject child( archiveTop, name );

        recursivelyAddChildren( child, depth, d, numChildren);
    }

    // Done - the archive closes itself
}

//


void recursivelyReadChildren( IObject& parent )
{
    unsigned int numChildren = parent.getNumChildren();
    std::cout << " has " << numChildren << " children"
              << std::endl;

    for (unsigned int ii=0; ii<numChildren; ii++)
    {
        IObject child = parent.getChild(ii);
        std::cout << "  " << child.getName();

        unsigned int expectedChildren = 2; 
        if (child.getName().substr(6,1) == "2")
            // bottom of the hierarchy
            expectedChildren = 0; 

        unsigned int children = child.getNumChildren();
        ABCA_ASSERT( children == expectedChildren,
                     "Expected " << expectedChildren << " children " <<
                     "but found " << children );

        recursivelyReadChildren( child );
    }

    return;
}


void readDeepHierarchy(const std::string &archiveName)
{
    // Open an existing archive for reading. Indicate that we want
    //   Alembic to throw exceptions on errors.
    IArchive archive( Alembic::AbcCoreHDF5::ReadArchive(),
                      archiveName, ErrorHandler::kThrowPolicy );
    IObject archiveTop = archive.getTop();

    // Determine the number of (top level) children the archive has
    const unsigned int numChildren = archiveTop.getNumChildren();
    std::cout << "The archive has " << numChildren << " children:"
              << std::endl;
    
    ABCA_ASSERT( numChildren == 2,
                 "Expected 2 children, found " << numChildren );

    // Iterate through them, print out their names
    for (unsigned int ii=0; ii<numChildren; ii++)
    {
        IObject child( archiveTop, archiveTop.getChildHeader(ii).getName() );
        std::cout << "  " << child.getName();

        recursivelyReadChildren( child );
    }


    // do it again to make sure we clean up after ourselves properly
    IArchive archive2( Alembic::AbcCoreHDF5::ReadArchive(), archiveName );
    IObject archiveTop2 = archive2.getTop();


    // Done - the archive closes itself
}

void readHierarchyMulti(const std::string &archiveName)
{

    Abc::IArchive a1(Alembic::AbcCoreHDF5::ReadArchive(), archiveName);
    {
        Abc::IArchive a2(Alembic::AbcCoreHDF5::ReadArchive(), archiveName);
    }

}

int main( int argc, char *argv[] )
{
    // Write and read a simple archive: ten children, with no
    //  properties
    try
    {
        std::string archiveName("flatHierarchy.abc");
        writeFlatHierarchy ( archiveName );
        readFlatHierarchy  ( archiveName );
    }
    catch (char * str )
    {
        std::cout << "Exception raised: " << str;
        std::cout << " during *FlatHierarchy tests" << std::endl;
        return 1;
    }

    // Write and read a slightly more complex archive: two children,
    //  with two children each, and two further children (for a
    //  total of a three-deep hierarchy with 14 total objects
    //  and eight leaf children)
    try
    {
        std::string archiveName("threeDeepHierarchy.abc");
        writeThreeDeepHierarchy ( archiveName );
        readDeepHierarchy  ( archiveName );
    }
    catch (char * str )
    {
        std::cout << "Exception raised: " << str;
        std::cout << " during *ThreeDeeptHierarchy tests" << std::endl;
        return 1;
    }

    {
        std::string archiveName("threeDeepHierarchy.abc");
        readHierarchyMulti(archiveName);
    }

    return 0;
}
