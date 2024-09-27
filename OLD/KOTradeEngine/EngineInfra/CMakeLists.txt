include_directories(
#  /usr/local/boost/1.60.0/include
  /usr/include/libxml2
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${VELIO_INCLUDE_DIRS}
)

add_library(
  EngineInfra
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/ContractAccount.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EngineEvent.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ErrorHandler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FigureEvent.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/HCScheduler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/HistoricDataRegister.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/KOEpochTime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/KOOrder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/KOScheduler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/OrderConfirmEvent.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/OrderRiskChecker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/PositionServerConnection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/PriceUpdateEvent.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SchedulerBase.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SchedulerConfig.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SimpleLogger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SimPositionServer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SimulationExchange.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/StaticDataHandler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SystemClock.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TradeEngineBase.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/WakeupEvent.cpp
)

target_link_libraries(
	EngineInfra
	DataPrinter
)

add_library(
  DataPrinter
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../DataPrinter/DataPrinter.cpp
)
