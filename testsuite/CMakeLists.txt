list( APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

##
# c/c++ std
##
include( CheckSTD )

set( TESTAPPS allocate 
    portTypeException dynallocate stdlibsupport split 
     join lambdatest allocate_s foreach peekrange  
     partitionTest parallel peek 
     bracketsTest uniform chain 
     splitchain staticLongSplitChain 
     staticSplitJoin staticSplitChainJoin 
     staticJoin staticContJoin 
     fitInCacheLine
     isExtClass
     doesntFitInCacheLine
     isInlineClass
     isExtArray
     isExtAlloc 
     isInlineNonClass 
     allocateSendPush 
     allocateSendRandomPush 
     allocatePopPush 
     allocatePopInternal 
     allocatePopInternalObject
     allocatePopExternal 
     parallelchain 
     ksettest
     multiPrint
     fixedMatchTest 
     splitchainRetStruct 
     staticContJoinRetStruct
     staticJoinRetStruct
     staticLongSplitChainRetStruct
     staticSplitChainJoinRetStruct
     staticSplitJoinRetStruct 
     chainMultiplePorts
     fixedAllocate
     noPortException
     twiceAdd
     disconnectedGraph 
     allocate_s_string
     allpush
     nonTrivialAllocatorPop
     nonTrivialAllocatorPopExternal
     vectorAlloc
     stringAlloc
     )

if( BUILDRANDOM )
list( APPEND TESTAPP gamma uniform gaussian exponential sequential ) 
endif( BUILDRANDOM )

foreach( APP ${TESTAPPS} )
 add_executable( ${APP} "${APP}.cpp" )
 target_link_libraries( ${APP}  
                                     raft
                                     demangle
                                     affinity
                                     ${CMAKE_THREAD_LIBS_INIT} 
                                     ${CMAKE_QTHREAD_LIBS}
                                     )
 if( VL_LIB )                                 
 target_link_libraries( ${APP} ${VL_LIB} )
 endif( VL_LIB )
 if( M5_LIB )                                 
     target_link_libraries( ${APP} ${M5_LIB} )
 endif( M5_LIB )
 add_test( NAME "${APP}_test" COMMAND ${APP} )
endforeach( APP ${TESTAPPS} )

file( COPY alice.txt
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR} )
