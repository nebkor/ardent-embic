//-*****************************************************************************
//
// Copyright (c) 2009-2010,
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

#include <Alembic/AbcCoreAbstract/CompoundPropertyReader.h>
#include <Alembic/AbcCoreAbstract/ScalarPropertyReader.h>
#include <Alembic/AbcCoreAbstract/ArrayPropertyReader.h>

namespace Alembic {
namespace AbcCoreAbstract {
namespace v1 {

//-*****************************************************************************
CompoundPropertyReader::~CompoundPropertyReader()
{
    // Nothing
}

//-*****************************************************************************
BasePropertyReaderPtr
CompoundPropertyReader::getProperty( const std::string &iName )
{
    const PropertyHeader *header = getPropertyHeader( iName );
    if ( !header )
    {
        return BasePropertyReaderPtr();
    }
    else
    {
        switch ( header->getPropertyType() )
        {
        default:
        case kScalarProperty:
            return getScalarProperty( header->getName() );
        case kArrayProperty:
            return getArrayProperty( header->getName() );
        case kCompoundProperty:
            return getCompoundProperty( header->getName() );
        }
    }        
}

//-*****************************************************************************
ScalarPropertyReaderPtr
CompoundPropertyReader::getScalarProperty( size_t i )
{
    // This will throw if bad index.
    const PropertyHeader &header = getPropertyHeader( i );

    if ( header.getPropertyType() != kScalarProperty )
    {
        return ScalarPropertyReaderPtr();
    }
    else
    {
        return getScalarProperty( header.getName() );
    }
}

//-*****************************************************************************
ArrayPropertyReaderPtr
CompoundPropertyReader::getArrayProperty( size_t i )
{
    // This will throw if bad index.
    const PropertyHeader &header = getPropertyHeader( i );

    if ( header.getPropertyType() != kArrayProperty )
    {
        return ArrayPropertyReaderPtr();
    }
    else
    {
        return getArrayProperty( header.getName() );
    }
}

//-*****************************************************************************
CompoundPropertyReaderPtr
CompoundPropertyReader::getCompoundProperty( size_t i )
{
    // This will throw if bad index.
    const PropertyHeader &header = getPropertyHeader( i );

    if ( header.getPropertyType() != kCompoundProperty )
    {
        return CompoundPropertyReaderPtr();
    }
    else
    {
        return getCompoundProperty( header.getName() );
    }
}

//-*****************************************************************************
BasePropertyReaderPtr
CompoundPropertyReader::getProperty( size_t i )
{
    // This will throw if bad index.
    const PropertyHeader &header = getPropertyHeader( i );
    
    switch ( header.getPropertyType() )
    {
    default:
    case kScalarProperty:
        return getScalarProperty( header.getName() );
    case kArrayProperty:
        return getArrayProperty( header.getName() );
    case kCompoundProperty:
        return getCompoundProperty( header.getName() );
    }
}

} // End namespace v1
} // End namespace AbcCoreAbstract
} // End namespace Alembic
