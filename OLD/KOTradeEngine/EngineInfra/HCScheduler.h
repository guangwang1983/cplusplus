#ifndef HCScheduler_H
#define HCScheduler_H

#include "SchedulerBase.h"
#include "SimpleLogger.h"

#include <framework/Model.h>
#include <framework/IModelContext.h>
#include <base-lib/VelioSessionManager.h>

namespace KO
{

class HCScheduler : public SchedulerBase,
					public Model
{
public:
    HCScheduler(SchedulerConfig &cfg, VelioSessionManager* _pVelioSessionManager, bool bIsLiveTrading);
    ~HCScheduler();

    void KOInit();

    void signalUpdate(TimeSeries<SIGNAL_STRUCT>* pSignalProvider) {};
    void RFEUpdate(short nodeKey, MESSAGE_HEADER* hdr, REQUEST_FOR_EXECUTION_STRUCT* pRFE) {};
    void RFEUpdateWC(short nodeKey, MESSAGE_HEADER* hdr, REQUEST_FOR_EXECUTION_STRUCT* pRFE){}
    void RFEPriceUpdate(short nodeKey, RFE_PRICE_STRUCT* pRFEP) {};
    void skewUpdate(double bidSkew, double askSkew, bool increaseRisk) {};
    void skewSideUpdate(bool bidEnabled, bool askEnabled) {};
    void eventQueueIsEmpty() {};
    void bBookEnabledUpdate(bool bBookEnabled) {};
	void initialize ();
	
	void bookUpdate(Book * pBook);
	void futureTickerUpdate(FUTURES_TICKER_ENTRY* ticker);
	void globalPositionUpdate(POSITION_DATA_STRUCT* pPosition);
    void orderUpdate(Order* order);

    static int updateTimer(lbm_context_t *ctx, const void * data);
    static int socketCallback(lbm_context_t *ctx, lbm_handle_t handle, lbm_ulong_t ev, void *clientd);

    void onTimer();

    KOEpochTime cgetCurrentTime();

	void addInstrument(string sKOProductName, string sHCProductName, InstrumentType eInstrumentType);

    bool bsubmitOrder(KOOrderPtr pOrder, double dPrice, long iQty);
    bool bchangeOrderPrice(KOOrderPtr pOrder, double dNewPrice);
    bool bchangeOrder(KOOrderPtr pOrder, double dNewPrice, long iNewQty);
    bool bdeleteOrder(KOOrderPtr pOrder);    
    void checkOrderStatus(KOOrderPtr pOrder);

    void registerPositionServerSocket(int socketID);
    void unregisterPositionServerSocket(int socketID);

    bool bisLiveTrading();
    long igetTotalOpenBuy(int iProductCID);
    long igetTotalOpenSell(int iProductCID);
    long igetAccountPosition(string sAccount, long iProductCID);

    void addNewOrderConfirmCall(SimulationExchange* pSimulationExchange, KOOrderPtr pOrder, KOEpochTime cCallTime);
private:
    struct RegisteredProduct
    {
        string sProductName;
        string sHCProductName;
        string sExchange;
        InstrumentType eInstrumentType;
    };

	vector<RegisteredProduct> _vRegisteredProducts;
    bool _bSchedulerInitialised;
    bool _bHCTimerInitialised;
    bool _bIsLiveTrading;

    map<int, KOOrderPtr> _mInternalOrdersMap;

    VelioSessionManager* _pVelioSessionManager;

    string _sModelName;

/* Position Server */
    boost::shared_ptr<PositionServerConnection> _pPositionServerConnection;
/* Position Server End */
};


};


#endif
