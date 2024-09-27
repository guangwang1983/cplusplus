#ifndef KO_FX_MODEL_FACTORY_MULTI_BOOK_H
#define KO_FX_MODEL_FACTORY_MULTI_BOOK_H

#include <framework/Model.h>
#include <ibook/FxBook.h>
#include <ibook/FuturesBook.h>
#include "HCFXSchedulerMultiBook.h"
#include "SchedulerBase.h"
#include "SimpleLogger.h"
#include "SchedulerConfig.h"

namespace KO
{

class KOFXModelFactoryMultiBook : public ModelFactory, public BookFactory
{
public:
    KOFXModelFactoryMultiBook(SchedulerConfig& cfg, bool bIsLiveTrading) : _cSchedulerCfg(cfg)
    {
        _pHCScheduler = 0;
        _bIsLiveTrading = bIsLiveTrading;
    };

    virtual ~KOFXModelFactoryMultiBook()
    {
        if(_pHCScheduler != 0)
        {
            delete _pHCScheduler;
            _pHCScheduler = 0;
        }
    };

    virtual Model* createModel(enum HC_GEN::MARKET_TYPE market, const std::string& instrument, bool priceOnly, HC::ecn_t ecn, HC::instrumentkey_t instrumentKey) override;

    virtual IBook * createBook(const enum HC_GEN::MARKET_TYPE market, const HC::source_t sourcePriceID, const HC::instrumentkey_t instrumentKey) override;

private:
    HCFXSchedulerMultiBook* _pHCScheduler;
    SchedulerConfig& _cSchedulerCfg;
    bool _bIsLiveTrading;
};

IBook * KOFXModelFactoryMultiBook::createBook(const enum HC_GEN::MARKET_TYPE market, const HC::source_t sourcePriceID, const HC::instrumentkey_t instrumentKey)
{
    (void) market;
    (void) sourcePriceID;
cerr << "in create book for insturment " << instrumentKey << " source price ID " << sourcePriceID << "\n";

    IBook * book = 0 ;

cerr << "creating new book \n";
    if(market == HC_GEN::FX)
    {
        book = new FxBook(instrumentKey, sourcePriceID);
cerr << "adding sub book for fx instrument \n";
        _pHCScheduler->addSubBookForInstrument(instrumentKey, sourcePriceID);
    }
    else
    {
        book = new FuturesBook(instrumentKey);
    }

    return book;
}

Model* KOFXModelFactoryMultiBook::createModel(enum HC_GEN::MARKET_TYPE market, const std::string& instrument, bool priceOnly, HC::ecn_t ecn, HC::instrumentkey_t instrumentKey)
{
cerr << "in create model \n";

    (void) priceOnly;
    (void) market;
    (void) ecn;

    if(_pHCScheduler == 0)
    {
cerr << "creating hc scheduler \n";
        _pHCScheduler = new HCFXSchedulerMultiBook(_cSchedulerCfg, _bIsLiveTrading);
    }

cerr << "instrument is " << instrument << "\n";
    bool bProductFound = false;
cerr << "_cSchedulerCfg.vProducts.size() " << _cSchedulerCfg.vHCProducts.size() << "\n";
    for(unsigned int i = 0; i != _cSchedulerCfg.vHCProducts.size(); i++)
    {
        if(instrument.find("SO3") != std::string::npos)
        {
            string adjustedHCProduct = instrument;
            adjustedHCProduct.replace(0,3,"L  ");

cerr << "comparing against " << _cSchedulerCfg.vHCProducts[i] << "\n";
            if(_cSchedulerCfg.vHCProducts[i] == adjustedHCProduct)
            {
                bProductFound = true;
cerr << "adding " << _cSchedulerCfg.vHCProducts[i] << "\n";
                _pHCScheduler->addInstrument(i, instrumentKey);
cerr << "added \n";
                break;
            }
        }
        else if(instrument.find("SR3") != std::string::npos)
        {
            string adjustedHCProduct = instrument;
            adjustedHCProduct.replace(0,3,"GE");

cerr << "Adjust instrument to " << adjustedHCProduct << "\n";
cerr << "comparing against " << _cSchedulerCfg.vHCProducts[i] << "\n";
            if(_cSchedulerCfg.vHCProducts[i] == adjustedHCProduct)
            {
                bProductFound = true;
cerr << "adding " << _cSchedulerCfg.vHCProducts[i] << "\n";
                _pHCScheduler->addInstrument(i, instrumentKey);
cerr << "added \n";
                break;
            }
        }
        else if(instrument.find("CRA") != std::string::npos)
        {
            string adjustedHCProduct = instrument;
            adjustedHCProduct.replace(0,3,"BAX");

cerr << "Adjust instrument to " << adjustedHCProduct << "\n";
cerr << "comparing against " << _cSchedulerCfg.vHCProducts[i] << "\n";
            if(_cSchedulerCfg.vHCProducts[i] == adjustedHCProduct)
            {
                bProductFound = true;
cerr << "adding " << _cSchedulerCfg.vHCProducts[i] << "\n";
                _pHCScheduler->addInstrument(i, instrumentKey);
cerr << "added \n";
                break;
            }
        }
        else if(instrument.find("GE") != std::string::npos)
        {
cerr << "Ignore " << instrument << "\n";
        }
        else
        {
cerr << "comparing against " << _cSchedulerCfg.vHCProducts[i] << "\n";
            if(_cSchedulerCfg.vHCProducts[i] == instrument)
            {
                bProductFound = true;
cerr << "adding " << _cSchedulerCfg.vHCProducts[i] << "\n";
                _pHCScheduler->addInstrument(i, instrumentKey);
cerr << "added \n";
                break;
            }
        }
    }

    if(bProductFound == true)
    {
cerr << "product found. return scheduler \n";
        return _pHCScheduler;
    }
    else
    {
        return 0;
    }
};

};

#endif /* KO_MODEL_FACTORY_H */
