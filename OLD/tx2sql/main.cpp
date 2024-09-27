#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <glob.h>
#include <vector>
#include <limits.h>

/*
 *   Include directly the different
 *     headers from cppconn/ and mysql_driver.h + mysql_util.h
 *       (and mysql_connection.h). This will reduce your build time!
 *       */
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;
using namespace sql;
using std::vector;

string sStrMonthToIntMonth(const string& sMonth)
{
    if(sMonth == "Jan")
    {
        return "01";
    }
    else if(sMonth == "Feb")
    {
        return "02";
    }
    else if(sMonth == "Mar")
    {
        return "03";
    }
    else if(sMonth == "Apr")
    {
        return "04";
    }
    else if(sMonth == "May")
    {
        return "05";
    }
    else if(sMonth == "Jun")
    {
        return "06";
    }
    else if(sMonth == "Jul")
    {
        return "07";
    }
    else if(sMonth == "Aug")
    {
        return "08";
    }
    else if(sMonth == "Sep")
    {
        return "09";
    }
    else if(sMonth == "Oct")
    {
        return "10";
    }
    else if(sMonth == "Nov")
    {
        return "11";
    }
    else if(sMonth == "Dec")
    {
        return "12";
    }
}

long iCalculateEpochMS(const string& sStrTimeStamp)
{
    string slinuxTimeStramp = sStrTimeStamp.substr(0,5) + sStrMonthToIntMonth(sStrTimeStamp.substr(5,3)) + sStrTimeStamp.substr(8,12);

    struct tm tm;
    time_t ts;
    memset(&tm, 0, sizeof(struct tm));
    
    strptime(slinuxTimeStramp.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
    ts = mktime(&tm);

    return (long)ts * 1000000 + (long)atoi(sStrTimeStamp.substr(21,6).c_str()); 
}

int main(int argc, char *argv[])
{
    sql::Driver *driver;
    sql::Connection *con;
    sql::Statement *stmt;
    sql::ResultSet *res;

    driver = get_driver_instance();
    con = driver->connect("localhost", "levelup", "Lxd34#4%d!dR");
    con->setSchema("levelup");

    stringstream cStringStream;
    srand(time(0));

    string sFolder = argv[1];

    vector<string> vConfigs;

    cStringStream.str("");
    cStringStream << sFolder << "/configs.csv";

    ifstream ifsConfigsNameFile (cStringStream.str().c_str());

    if(ifsConfigsNameFile.is_open())
    {
        stmt = con->createStatement();

        while(!ifsConfigsNameFile.eof())
        {
            char sNewLine[2048];
            ifsConfigsNameFile.getline(sNewLine, sizeof(sNewLine));
            if(strlen(sNewLine) > 0)
            {
                vConfigs.push_back(sNewLine);
            }
        }

        for(vector<string>::iterator itr = vConfigs.begin();
            itr != vConfigs.end();
            itr++)
        {
            glob_t glob_result;
            memset(&glob_result, 0, sizeof(glob_result));

            cStringStream.str("");
            cStringStream << sFolder << "/" << *itr << "/" << "Transaction*";

            int return_value = glob(cStringStream.str().c_str(), GLOB_TILDE, NULL, &glob_result);

            if(return_value == 0)
            {
                for(size_t i = 0; i < glob_result.gl_pathc; ++i)
                {
//                    cout << string(glob_result.gl_pathv[i]) << "\n";

                    ifstream ifsTxFile(string(glob_result.gl_pathv[i]).c_str());
                    if(ifsTxFile.is_open())
                    {
                        while(!ifsTxFile.eof())
                        {
                            char sNewLine[2048];
                            ifsTxFile.getline(sNewLine, sizeof(sNewLine));
                            if(string(sNewLine).find("TIME") == string::npos && strlen(sNewLine) > 2)
                            {
                                string sTime;
                                string sProduct;
                                string sQty;
                                string sPrice;

                                std::istringstream cDailyStatStream(sNewLine);

                                std::getline(cDailyStatStream, sTime, ';');
                                std::getline(cDailyStatStream, sProduct, ';');
                                std::getline(cDailyStatStream, sQty, ';');
                                std::getline(cDailyStatStream, sPrice, ';');
                            
//                                cerr << sTime << "\n";
   
                                long iepochTimeStamp = iCalculateEpochMS(sTime);

//                                cerr << sTime << " " << iepochTimeStamp << " " << sProduct << " " << sQty << " " << sPrice << "\n";

                                stringstream cSQLCommandStream;
                                cSQLCommandStream << "INSERT INTO TRANSACTIONS VALUES ('" << sFolder << "', '" << *itr << "', " << iepochTimeStamp << ", '" << sProduct << "', " << sPrice << ", " << sQty << ")";
                                stmt->execute(cSQLCommandStream.str());
                            }
                        }
                    }
                }
            }
        }

        delete stmt;
    }


    
/*
    sql::Driver *driver;
    sql::Connection *con;
    sql::Statement *stmt;
    sql::ResultSet *res;

    driver = get_driver_instance();
    con = driver->connect("localhost", "levelup", "Lxd34#4%d!dR");
    con->setSchema("levelup");
*/

// ************** create table ***********************
//    stmt = con->createStatement();
//    stmt->execute("CREATE TABLE TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");

// *************** create unique table and insert rows *************
//    stmt = con->createStatement();
//    stmt->execute("CREATE TABLE IF NOT EXISTS TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");
//    stmt->execute("INSERT INTO TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V VALUES (1533671936177130, '6CU8', 0.77175, 1), (1533671936177134, '6CU8', 0.77175, -1)");

// *************** get result *************************
//    stmt = con->createStatement();
//    res = stmt->executeQuery("SELECT * FROM TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V");
//    cout << "query executed \n";
    // loading 650000 rows takes 1.15 seconds
//    while (res->next()) 
//    {
//        cout << res->getString("time") << " ";
//        cout << res->getString("product") << " ";
//        cout << res->getString("price") << " ";
//        cout << res->getString("quantity") << "\n";
//    }

// *************** get result sorted *************************
//    stmt = con->createStatement();
//    res = stmt->executeQuery("SELECT * FROM TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V ORDER BY time ASC");
//    cout << "sorted query executed \n";
    // loading 650000 rows takes 1.56 seconds
//    while (res->next()) 
//    {
//        cout << res->getString("time") << " ";
//        cout << res->getString("product") << " ";
//        cout << res->getString("price") << " ";
//        cout << res->getString("quantity") << "\n";
//    }

// *************** get filtered result sorted ***************
//    stmt = con->createStatement();
//    res = stmt->executeQuery("SELECT * FROM TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V where time < 1533671936151931 AND time > 1533671936108644 ORDER BY time ASC");
//    cout << " filtered sorted query executed \n";
    // get 50000 out of 650000 rows from unsorted table takes 0.496 seconds
    // get 50000 out of 650000 rows from sorted table takes 0.481 seconds
    //
// *************** get filtered result ***************
//    stmt = con->createStatement();
//    res = stmt->executeQuery("SELECT * FROM TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V where time < 1533671936151931 AND time > 1533671936108644");
//    cout << " filtered query executed \n";
    // get 50000 out of 650000 rows from unsorted table takes 0.466 seconds
    // get 50000 out of 650000 rows from sorted table takes

// **************** sort table ******************************
//    stmt = con->createStatement();
//    stmt->execute("ALTER TABLE TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V ORDER BY time ASC");
//    cout << "tabled sorted \n";
    // sort 650000 rows take 5.5

// ****************** insert 100000 rows ********************
/*
    stmt = con->createStatement();
    stmt->execute("CREATE TABLE IF NOT EXISTS TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");

    stringstream cStringStream;
    cStringStream << "INSERT INTO TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V VALUES";

    // 100000 is about as big as mysql allows   
    // adding 100000 rows takes 0.91 seconds 
    for(int i = 0; i < 100000; i++)
    {
        if(i != 0)
        {
            cStringStream << "," ; 
        }

        cStringStream << "(" << (1533671936177130 + rand() % 500000 - 250000) << ", '6CU8', 0.77175, 1)";
    }

    stmt->execute(cStringStream.str());
*/

// ****************** insert 100000 rows with sequential key ********************
/*
    stmt = con->createStatement();
    stmt->execute("CREATE TABLE IF NOT EXISTS TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT, PRIMARY KEY(time))");

    stringstream cStringStream;
    cStringStream << "INSERT INTO TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V VALUES";

    // 100000 is about as big as mysql allows   
    // adding 100000 rows takes 0.96 seconds
    for(int i = 0; i < 100000; i++)
    {
        if(i != 0)
        {
            cStringStream << "," ; 
        }

        cStringStream << "(" << (1533671936177130 + i) << ", '6CU8', 0.77175, 1)";
    }

    stmt->execute(cStringStream.str());
*/

// ****************** replace 100000 rows with sequential key ********************
/*
    stmt = con->createStatement();
    stmt->execute("CREATE TABLE IF NOT EXISTS TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT, PRIMARY KEY(time))");

    stringstream cStringStream;
    cStringStream << "REPLACE INTO TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V VALUES";

    // 100000 is about as big as mysql allows  
    // replace 100000 if the key is not there takes 0.96 seconds same as insert 
    // otherwise takes 1.32
    for(int i = 0; i < 100000; i++)
    {
        if(i != 0)
        {
            cStringStream << "," ; 
        }

        cStringStream << "(" << (1533671936177130 + i) << ", '6CU8', 0.77175, 2)";
    }

    stmt->execute(cStringStream.str());


    delete stmt;
*/
/*
try {
  sql::Driver *driver;
  sql::Connection *con;
  sql::Statement *stmt;
  sql::ResultSet *res;

  driver = get_driver_instance();
  con = driver->connect("levelup-dev", "levelup", "Lxd34#4%d!dR");
  con->setSchema("test");

  stmt = con->createStatement();
  res = stmt->executeQuery("SELECT 'Hello World!' AS _message"); // replace with your statement
  while (res->next()) {
    cout << "\t... MySQL replies: ";
    cout << res->getString("_message") << endl;
    cout << "\t... MySQL says it again: ";
    cout << res->getString(1) << endl;
  }
  delete res;
  delete stmt;
  delete con;

} 
catch (sql::SQLException &e) {
  cout << "# ERR: SQLException in " << __FILE__;
  cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
  cout << "# ERR: " << e.what();
  cout << " (MySQL error code: " << e.getErrorCode();
  cout << ", SQLState: " << e.getSQLState() << " )" << endl;
}
*/

cout << endl;

return EXIT_SUCCESS;
}
