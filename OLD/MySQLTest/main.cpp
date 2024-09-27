#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <time.h>

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

int main(void)
{
    srand(time(0));

    sql::Driver *driver;
    sql::Connection *con;
    sql::Statement *stmt;
    sql::ResultSet *res;

    driver = get_driver_instance();
    con = driver->connect("localhost", "levelup", "Lxd34#4%d!dR");
    con->setSchema("levelup");


// ************** create database ********************
//    stmt = con->createStatement();
//    stmt->execute("CREATE DATABASE IF NOT EXISTS TEST_DB");

// ************** create table ***********************
//    stmt = con->createStatement();
//    stmt->execute("CREATE TABLE TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");

// *************** create unique table and insert rows *************
//    stmt = con->createStatement();
//    stmt->execute("CREATE TABLE IF NOT EXISTS TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");
//    stmt->execute("INSERT INTO TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V VALUES (1533671936177130, '6CU8', 0.77175, 1), (1533671936177134, '6CU8', 0.77175, -1)");

// *************** insert records ***************
//    stmt = con->createStatement();
//    stmt->execute("CREATE TABLE IF NOT EXISTS TRANSACTIONS (folder TEXT, config TEXT, time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");
//    stmt->execute("ALTER TABLE TRANSACTIONS ADD INDEX (folder, config)")
//    for(int j = 0; j < 100; j++)
//    {    
//cerr << "j is " << j << "\n";
//        stringstream cStringStream;
//        cStringStream << "INSERT INTO TRANSACTIONS VALUES";

        // 100000 is about as big as mysql allows   
        // adding 100000 rows takes 0.91 seconds
//        for(int i = 0; i < 3000; i++)
//        {
//            if(i != 0)
//            {
//                cStringStream << "," ; 
//            }

//            cStringStream << " ('/apps/levelup/production/library', 'TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V_" << j << "', " << (1533671936177130 + rand() % 500000 - 250000) << ", '6CU8', 0.77175, 1)";
//        }

//        cout << cStringStream.str();

//        stmt->execute(cStringStream.str());
//    }

//    delete stmt;

//*************** get result *************************
    stmt = con->createStatement();
    res = stmt->executeQuery("SELECT * FROM TRANSACTIONS where config = 'TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V_1' AND folder = '/apps/levelup/production/library'");
    cout << "query executed \n";
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
    
// *************** get filtered result sorted ***************
//    stmt = con->createStatement();
//    res = stmt->executeQuery("SELECT * FROM TX where config = 'TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V_88' AND time < 1533671936151931 AND time > 1533671936108644 ORDER BY time ASC");
//    cout << " filtered sorted query executed \n";
    // get 50000 out of 650000 rows from unsorted table takes 0.496 seconds
    // get 50000 out of 650000 rows from sorted table takes 0.481 seconds

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
    for(int j = 0; j < 100; j++)
    {    
cerr << "j is " << j << "\n";
        stmt = con->createStatement();
        stmt->execute("CREATE TABLE IF NOT EXISTS TX (config TEXT, time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT)");

        stringstream cStringStream;
        cStringStream << "INSERT INTO TX VALUES";

        // 100000 is about as big as mysql allows   
        // adding 100000 rows takes 0.91 seconds
        for(int i = 0; i < 3000; i++)
        {
            if(i != 0)
            {
                cStringStream << "," ; 
            }

            cStringStream << " ('TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V_" << j << "', " << (1533671936177130 + rand() % 500000 - 250000) << ", '6CU8', 0.77175, 1)";
        }

//        cout << cStringStream.str();

        stmt->execute(cStringStream.str());
    
        delete stmt;
    }
*/

// ****************** insert 100000 rows with sequential key ********************
/*
    stmt = con->createStatement();
    stmt->execute("CREATE TABLE IF NOT EXISTS TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V (time BIGINT, product CHAR(10), price DOUBLE, quantity BIGINT, PRIMARY KEY(time))");

    stringstream cStringStream;
    cStringStream << "INSERT INTO TM_SLSL_XEUR_FGBM_0_XCME_ZF_0_900D_2_5T_2_0E10V VALUES";

    // 100000 is about as big as mysql allows   
    // adding 100000 rows takes 0.96 seconds
    for(int i = 0; i < 2; i++)
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
*/

//    delete stmt;

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

//    cout << endl;

    return EXIT_SUCCESS;
}
