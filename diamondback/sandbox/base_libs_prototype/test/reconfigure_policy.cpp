/***************************************************************************
 *  test/reconfigure_policy.cpp
 *  --------------------
 * 
 *  Copyright (c) 2011, Edward T. Kaszubski ( ekaszubski@gmail.com )
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of seabee3-ros-pkg nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 **************************************************************************/

#include <base_libs/node.h>
#include <base_libs/reconfigure_policy.h>
#include <base_libs_prototype/TestConfig.h>
#include <base_libs_prototype/Test2Config.h>

typedef base_libs::ReconfigurePolicy<base_libs_prototype::TestConfig> _TestConfigReconfigurePolicy;
typedef base_libs::ReconfigurePolicy<base_libs_prototype::Test2Config> _Test2ConfigReconfigurePolicy;

BASE_LIBS_DECLARE_NODE( TestReconfigurePolicy, _TestConfigReconfigurePolicy, _Test2ConfigReconfigurePolicy )

BASE_LIBS_DECLARE_NODE_CLASS( TestReconfigurePolicy )
{
	BASE_LIBS_DECLARE_NODE_CONSTRUCTOR( TestReconfigurePolicy )
	{
		//
	}
	
	void spinFirst()
	{
		// call init() on all initable policies
		//initAll();
		
		// call init( ... ) on all initable policies
		//initAll( "reconfigure_namespace_name", std::string( "reconfigure_namespace_1" ) );
		
		// call init on just _TestConfigReconfigurePolicy
		// All ReconfigurePolicy instances use getMetaParamDef to look up the key "reconfigure_namespace_name" in the list of init() params
		// and default to "reconfigure_namspace" if no matching key is found. This value is then fed into a ParamReader as the name of a param.
		// The value of that param is then used as the reconfigure namespace.
		//
		// So, when the following line is called, a ParamReader will try to find "node_name/reconfigure_namespace_1" or default to "reconfigure" (let's call the result <reconfigure_ns>
		// Then a dynamic reconfigure server will be set up as: node_name/<reconfigure_ns>
		// If this function was given no arguments, the ParamReader would instead try to find "node_name/reconfigure_namespace" (since the default value for they meta-param key
		// "reconfigure_namespace_name", as defined in ReconfigurePolicy, is "reconfigure_namespace") or default to "reconfigure", etc
		_TestConfigReconfigurePolicy::init( "reconfigure_namespace_name", std::string( "reconfigure_namespace_1" ) );
		_TestConfigReconfigurePolicy::registerCallback( base_libs::auto_bind( &TestReconfigurePolicyNode::reconfigureCB1, this ) );
		
		// call init on just _Test2ConfigReconfigurePolicy
		_Test2ConfigReconfigurePolicy::init( "reconfigure_namespace_name", std::string( "reconfigure_namespace_2" ) );
		_Test2ConfigReconfigurePolicy::registerCallback( base_libs::auto_bind( &TestReconfigurePolicyNode::reconfigureCB2, this ) );
		
		// so now, when we run this node using the above inits, we can do:
		// ./node_name _reconfigure_namespace_1:=reconfigure1 _reconfigure_namespace_2:=reconfigure2
		// and our two dynamic reconfigure servers will work in harmony; the server for TestConfig
		// will show up on /node_name/reconfigure1 and the server for TestConfig2 will show up on
		// /node_name/reconfigure2
		
		//_TestConfigReconfigurePolicy::registerCallback( std::auto_bind<
	}
	
	void spinOnce()
	{
		std::cout << "." << std::flush;
	}
	
	void reconfigureCB1( _TestConfigReconfigurePolicy::_ReconfigureType & config, uint32_t level )
	{
		printf( "Got reconfigureCB1\n" );
	}
	
	void reconfigureCB2( _Test2ConfigReconfigurePolicy::_ReconfigureType & config, uint32_t level )
	{
		printf( "Got reconfigureCB2\n" );
	}
};

BASE_LIBS_INST_NODE( TestReconfigurePolicyNode, "test_reconfigure_policy_node" )