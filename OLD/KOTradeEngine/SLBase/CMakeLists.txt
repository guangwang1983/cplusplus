add_library(
  SLBase
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/SLBase.cpp
)

add_library(
  base
  STATIC
  ../base/EXMA.cpp
  ../base/FairValueExecution.cpp
  ../base/HedgeOrder.cpp
  ../base/Instrument.cpp
  ../base/IOCOrder.cpp
  ../base/MultiProductPricingBase.cpp
  ../base/Product.cpp
  ../base/QuoteOrder.cpp
  ../base/SMA.cpp
  ../base/Stdev.cpp
  ../base/SyntheticSpread.cpp
  ../base/WABSPricing.cpp
  ../base/WeightedStdev.cpp
)

target_link_libraries(
  SLBase
  base)
