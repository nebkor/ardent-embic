//-*****************************************************************************
//
// Copyright (c) 2009-2010,
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

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>

#include <iostream>

#include <pthread.h>

//-*****************************************************************************
using namespace ::Alembic::AbcGeom;

static const Abc::V3f g_vec( 1.0, 2.0, 3.0 );

//-*****************************************************************************
void *visitSimpleProperty( void* prop )
{
    Abc::IArrayProperty iProp = *(static_cast<Abc::IArrayProperty *>( prop ));
    if ( Abc::IV3fArrayProperty::matches( iProp.getHeader() ) &&
         iProp.getNumSamples() > 0 )
    {
        Abc::IV3fArrayProperty p( iProp.getPtr(), kWrapExisting );

        Abc::V3fArraySamplePtr samp = p.getValue();

        if ( samp->size() > 0 && (*samp)[0] == g_vec )
        {
            std::cout << "That is extremely unlikely!" << std::endl;
            std::cout << iProp.getObject().getFullName() << std::endl;
        }
    }
    return NULL;
}

//-*****************************************************************************
void visitProperties( ICompoundProperty iParent,
                      std::vector<pthread_t> &threads, size_t &tid )
{
    for ( size_t i = 0 ; i < iParent.getNumProperties() ; i++ )
    {
        PropertyHeader header = iParent.getPropertyHeader( i );

        if ( header.isCompound() )
        {
            visitProperties( ICompoundProperty( iParent, header.getName() ),
                             threads, tid );
        }
        else if ( header.isArray() )
        {
            IArrayProperty p( iParent, header.getName() );
            int64_t id = pthread_create( &threads[tid++], NULL,
                                         visitSimpleProperty, &p );
            std::cout << "spawned thread id " << id << std::endl;
        }
    }
}

//-*****************************************************************************
void visitObject( IObject iObj, std::vector<pthread_t>& threads, size_t tid )
{
    ICompoundProperty props = iObj.getProperties();
    visitProperties( props, threads, tid );

    // now the child objects
    for ( size_t i = 0 ; i < iObj.getNumChildren() ; i++ )
    {
        visitObject( IObject( iObj, iObj.getChildHeader( i ).getName() ),
                     threads, tid );
    }
}

//-*****************************************************************************
//-*****************************************************************************
// DO IT.
//-*****************************************************************************
//-*****************************************************************************
int main( int argc, char *argv[] )
{
    if ( argc != 2 )
    {
        std::cerr << "USAGE: " << argv[0] << " <AlembicArchive.abc>"
                  << std::endl;
        exit( -1 );
    }

    std::vector<pthread_t> threads( 50,000 );

    // Scoped.
    {
        IArchive archive( Alembic::AbcCoreHDF5::ReadArchive(),
                          argv[1] );
        visitObject( archive.getTop(), threads, 0 );
    }

    return 0;
}
