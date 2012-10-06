/*
 *  File:         tz_table.h
 *
 *  Author:       Beat Forster
 *
 *  Time zone information.
 *  (CURRENTLY NOT) Automatically generated with 'read_tzi'
 *  Conversion date: ??/??/??
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 */



#ifndef TZ_TABLE_H
#define TZ_TABLE_H

typedef enum {
  // special time zones
  tctx_tz_unknown,                  //   0
  tctx_tz_system,                   //   0
  tctx_tz_UTC,                      //   0    Coordinated Universal Time Europe, will be preferred

  // list of all Registry time zones
  tctx_tz_Afghanistan,              //   4.5  Kabul
  tctx_tz_AKST_AKDT,                //  -9    Alaska
  tctx_tz_AKST_AKDT_2006,           //  -9    Alaska
  tctx_tz_AKST_AKDT_2007,           //  -9    Alaska
  tctx_tz_HNY_NAY,                  //  -9    Alaska
  tctx_tz_HNY_NAY_2006,             //  -9    Alaska
  tctx_tz_HNY_NAY_2007,             //  -9    Alaska
  tctx_tz_Alaskan,                  //  -9    Alaska
  tctx_tz_Alaskan_2006,             //  -9    Alaska
  tctx_tz_Alaskan_2007,             //  -9    Alaska
  tctx_tz_Arab,                     //   3    Kuwait, Er Riad
  tctx_tz_Arabian,                  //   4    Abu Dhabi, Muskat
  tctx_tz_Arabic,                   //   3    Bagdad
  tctx_tz_AST_ADT,                  //  -4    Atlantik (Kanada)
  tctx_tz_AST_ADT_2006,             //  -4    Atlantik (Kanada)
  tctx_tz_AST_ADT_2007,             //  -4    Atlantik (Kanada)
  tctx_tz_HNA_HAA,                  //  -4    Atlantik (Kanada)
  tctx_tz_HNA_HAA_2006,             //  -4    Atlantik (Kanada)
  tctx_tz_HNA_HAA_2007,             //  -4    Atlantik (Kanada)
  tctx_tz_Atlantic,                 //  -4    Atlantik (Kanada)
  tctx_tz_Atlantic_2006,            //  -4    Atlantik (Kanada)
  tctx_tz_Atlantic_2007,            //  -4    Atlantik (Kanada)
  tctx_tz_ART_ARST,                 //  -3    Buenos Aires
  tctx_tz_ART_ARST_2006,            //  -3    Buenos Aires
  tctx_tz_ART_ARST_2007,            //  -3    Buenos Aires
  tctx_tz_ART_ARST_2008,            //  -3    Buenos Aires
  tctx_tz_ART_ARST_2009,            //  -3    Buenos Aires
  tctx_tz_AUS_Central,              //   9.5  Darwin
  tctx_tz_AUS_Eastern,              //  10    Canberra, Melbourne, Sydney
  tctx_tz_Azerbaijan,               //   4    Baku
  tctx_tz_Azores,                   //  -1    Azoren
  tctx_tz_Canada_Central,           //  -6    Saskatchewan
  tctx_tz_Cape_Verde,               //  -1    Kapverdische Inseln
  tctx_tz_Caucasus,                 //   4    Eriwan
  tctx_tz_ACST_ACDT,                //   9.5  Adelaide
  tctx_tz_Central_Australia,        //   9.5  Adelaide
  tctx_tz_Central_America,          //  -6    Zentralamerika
  tctx_tz_Central_Asia,             //   6    Astana, Dhaka
  tctx_tz_Central_Brazilian,        //  -4    Manaus
  tctx_tz_Central_Brazilian_2006,   //  -4    Manaus
  tctx_tz_Central_Brazilian_2007,   //  -4    Manaus
  tctx_tz_CET_CEST,                 //   1    Belgrad, Bratislava, Budapest, Ljubljana, Prag
  tctx_tz_MEZ_MESZ,                 //   1    Belgrad, Bratislava, Budapest, Ljubljana, Prag
  tctx_tz_Central_Europe,           //   1    Belgrad, Bratislava, Budapest, Ljubljana, Prag
  tctx_tz_Central_European,         //   1    Sarajevo, Skopje, Warschau, Zagreb
  tctx_tz_Central_Pacific,          //  11    Magadan, Salomonen, Neukaledonien
  tctx_tz_CST_CDT,                  //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_CST_CDT_2006,             //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_CST_CDT_2007,             //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_HNC_HAC,                  //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_HNC_HAC_2006,             //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_HNC_HAC_2007,             //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_Central,                  //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_Central_2006,             //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_Central_2007,             //  -6    Chicago, Dallas, Kansas City, Winnipeg
  tctx_tz_Central_Mexico,           //  -6    Guadalajara, Mexiko-Stadt, Monterrey - neu
  tctx_tz_China,                    //   8    Peking, Chongqing, Hongkong, Urumchi
  tctx_tz_Dateline,                 // -12    Internationale Datumsgrenze (Westen)
  tctx_tz_East_Africa,              //   3    Nairobi
  tctx_tz_AEST_AEDT,                //  10    Brisbane
  tctx_tz_East_Australia,           //  10    Brisbane
  tctx_tz_EET_EEST,                 //   2    Minsk
  tctx_tz_East_Europe,              //   2    Minsk
  tctx_tz_East_South_America,       //  -3    Brasilia
  tctx_tz_East_South_America_2006,  //  -3    Brasilia
  tctx_tz_East_South_America_2007,  //  -3    Brasilia
  tctx_tz_EST_EDT,                  //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_EST_EDT_2006,             //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_EST_EDT_2007,             //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_HNE_HAE,                  //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_HNE_HAE_2006,             //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_HNE_HAE_2007,             //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_Eastern,                  //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_Eastern_2006,             //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_Eastern_2007,             //  -5    New York, Miami, Atlanta, Detroit, Toronto
  tctx_tz_Egypt,                    //   2    Kairo
  tctx_tz_Ekaterinburg,             //   5    Jekaterinburg
  tctx_tz_Fiji,                     //  12    Fidschi, Kamtschatka, Marshall-Inseln
  tctx_tz_FLE,                      //   2    Helsinki, Kiew, Riga, Sofia, Tallinn, Wilna
  tctx_tz_Georgian,                 //   3    Tiflis
  tctx_tz_GMT,                      //   0    Dublin, Edinburgh, Lissabon, London
  tctx_tz_Greenland,                //  -3    Groenland
  tctx_tz_Greenwich,                //   0    Casablanca, Monrovia, Reykjavik
  tctx_tz_GTB,                      //   2    Athen, Bukarest, Istanbul
  tctx_tz_HST_HPT,                  // -10    Hawaii
  tctx_tz_HAST_HADT,                // -10    Hawaii
  tctx_tz_Hawaiian,                 // -10    Hawaii
  tctx_tz_India,                    //   5.5  Chennai, Kolkata, Mumbai, Neu-Delhi
  tctx_tz_Iran,                     //   3.5  Teheran
  tctx_tz_Iran_2005,                //   3.5  Teheran
  tctx_tz_Iran_2006,                //   3.5  Teheran
  tctx_tz_Israel,                   //   2    Jerusalem
  tctx_tz_Israel_2004,              //   2    Jerusalem
  tctx_tz_Israel_2005,              //   2    Jerusalem
  tctx_tz_Israel_2006,              //   2    Jerusalem
  tctx_tz_Israel_2007,              //   2    Jerusalem
  tctx_tz_Israel_2008,              //   2    Jerusalem
  tctx_tz_Israel_2009,              //   2    Jerusalem
  tctx_tz_Israel_2010,              //   2    Jerusalem
  tctx_tz_Israel_2011,              //   2    Jerusalem
  tctx_tz_Israel_2012,              //   2    Jerusalem
  tctx_tz_Israel_2013,              //   2    Jerusalem
  tctx_tz_Israel_2014,              //   2    Jerusalem
  tctx_tz_Israel_2015,              //   2    Jerusalem
  tctx_tz_Israel_2016,              //   2    Jerusalem
  tctx_tz_Israel_2017,              //   2    Jerusalem
  tctx_tz_Israel_2018,              //   2    Jerusalem
  tctx_tz_Israel_2019,              //   2    Jerusalem
  tctx_tz_Israel_2020,              //   2    Jerusalem
  tctx_tz_Israel_2021,              //   2    Jerusalem
  tctx_tz_Israel_2022,              //   2    Jerusalem
  tctx_tz_Israel_2023,              //   2    Jerusalem
  tctx_tz_Jordan,                   //   2    Amman
  tctx_tz_Korea,                    //   9    Seoul
  tctx_tz_Mexico,                   //  -6    Guadalajara, Mexiko-Stadt, Monterrey - alt
  tctx_tz_Mexico_2,                 //  -7    Chihuahua, La Paz, Mazatlan - alt
  tctx_tz_Mid_Atlantic,             //  -2    Mittelatlantik
  tctx_tz_Middle_East,              //   2    Beirut
  tctx_tz_Montevideo,               //  -3    Montevideo
  tctx_tz_MST_MDT,                  //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_MST_MDT_2006,             //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_MST_MDT_2007,             //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_HNR_HAR,                  //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_HNR_HAR_2006,             //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_HNR_HAR_2007,             //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_Mountain,                 //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_Mountain_2006,            //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_Mountain_2007,            //  -7    Denver, Salt Lake City, Calgary
  tctx_tz_Mountain_Mexico,          //  -7    Chihuahua, La Paz, Mazatlan - neu
  tctx_tz_Myanmar,                  //   6.5  Yangon (Rangun)
  tctx_tz_North_Central_Asia,       //   6    Almaty, Nowosibirsk
  tctx_tz_Namibia,                  //   2    Windhuk
  tctx_tz_Nepal,                    //   5.75 Katmandu
  tctx_tz_New_Zealand,              //  12    Auckland, Wellington
  tctx_tz_NST_NDT,                  //  -3.5  Neufundland
  tctx_tz_NST_NDT_2006,             //  -3.5  Neufundland
  tctx_tz_NST_NDT_2007,             //  -3.5  Neufundland
  tctx_tz_HNT_HAT,                  //  -3.5  Neufundland
  tctx_tz_HNT_HAT_2006,             //  -3.5  Neufundland
  tctx_tz_HNT_HAT_2007,             //  -3.5  Neufundland
  tctx_tz_Newfoundland,             //  -3.5  Neufundland
  tctx_tz_Newfoundland_2006,        //  -3.5  Neufundland
  tctx_tz_Newfoundland_2007,        //  -3.5  Neufundland
  tctx_tz_North_Asia_East,          //   8    Irkutsk, Ulan Bator
  tctx_tz_North_Asia,               //   7    Krasnojarsk
  tctx_tz_Pacific_SA,               //  -4    Santiago
  tctx_tz_PST_PDT,                  //  -8    Pacific (USA, Kanada)
  tctx_tz_PST_PDT_2006,             //  -8    Pacific (USA, Kanada)
  tctx_tz_PST_PDT_2007,             //  -8    Pacific (USA, Kanada)
  tctx_tz_HNP_HAP,                  //  -8    Pacific (USA, Kanada)
  tctx_tz_HNP_HAP_2006,             //  -8    Pacific (USA, Kanada)
  tctx_tz_HNP_HAP_2007,             //  -8    Pacific (USA, Kanada)
  tctx_tz_Pacific,                  //  -8    Pacific (USA, Kanada)
  tctx_tz_Pacific_2006,             //  -8    Pacific (USA, Kanada)
  tctx_tz_Pacific_2007,             //  -8    Pacific (USA, Kanada)
  tctx_tz_Pacific_Mexico,           //  -8    Tijuana, Niederkalifornien (Mexiko)
  tctx_tz_Romance,                  //   1    Bruessel, Kopenhagen, Madrid, Paris
  tctx_tz_Russian,                  //   3    Moskau, St. Petersburg, Wolgograd
  tctx_tz_SA_Eastern,               //  -3    Buenos Aires, Georgetown (OLD)
  tctx_tz_SA_Pacific,               //  -5    Bogota, Lima, Quito, Rio Branco
  tctx_tz_SA_Western,               //  -4    Caracas, La Paz
  tctx_tz_Samoa,                    // -11    Midway-Inseln, Samoa
  tctx_tz_SE_Asia,                  //   7    Bangkok, Hanoi, Jakarta
  tctx_tz_Singapore,                //   8    Kuala Lumpur, Singapur
  tctx_tz_South_Africa,             //   2    Harare, Praetoria
  tctx_tz_Sri_Lanka,                //   5.5  Sri Jayawardenepura
  tctx_tz_Taipei,                   //   8    Taipeh
  tctx_tz_Tasmania,                 //  10    Hobart
  tctx_tz_Tokyo,                    //   9    Osaka, Sapporo, Tokio
  tctx_tz_Tonga,                    //  13    Nuku'alofa
  tctx_tz_US_Eastern,               //  -5    Indiana (Ost)
  tctx_tz_US_Mountain,              //  -7    Arizona
  tctx_tz_Vladivostok,              //  10    Wladiwostok
  tctx_tz_West_Australia,           //   8    Perth
  tctx_tz_West_Australia_2005,      //   8    Perth
  tctx_tz_West_Australia_2006,      //   8    Perth
  tctx_tz_West_Australia_2007,      //   8    Perth
  tctx_tz_West_Central_Africa,      //   1    West-Zentralafrika
  tctx_tz_WET_WEST,                 //   1    Amsterdam, Berlin, Bern, Rom, Stockholm, Wien
  tctx_tz_West_Europe,              //   1    Amsterdam, Berlin, Bern, Rom, Stockholm, Wien
  tctx_tz_West_Asia,                //   5    Islamabad, Karatschi, Taschkent
  tctx_tz_West_Pacific,             //  10    Guam, Port Moresby
  tctx_tz_Yakutsk,                  //   9    Jakutsk

  // http://www.timeanddate.com/library/abbreviations/timezones
  tctx_tz_A,                        //   1    Alpha Time Zone                  <Military>
  tctx_tz_ACDT,                     //  10.5  Australian Central Daylight Time   Australia
  tctx_tz_ACST,                     //   9.5  Australian Central Standard Time   Australia
  tctx_tz_ADT,                      //  -3    Atlantic Daylight Time            North America
  tctx_tz_AEDT,                     //  11    Australian Eastern Daylight Time   Australia
  tctx_tz_AEST,                     //  10    Australian Eastern Standard Time   Australia
  tctx_tz_AKDT,                     //  -8    Alaska Daylight Time              North America
  tctx_tz_AKST,                     //  -9    Alaska Standard Time              North America
  tctx_tz_AST,                      //  -4    Atlantic Standard Time            North America
  tctx_tz_AWST,                     //   8    Australian Western Standard Time   Australia
  tctx_tz_B,                        //   2    Bravo Time Zone                  <Military>
  tctx_tz_BST,                      //   1    British Summer Time               Europe
  tctx_tz_C,                        //   3    Charlie Time Zone                <Military>
  tctx_tz_CDT,                      //  -5    Central Daylight Time             North America
  tctx_tz_CEST,                     //   2    Central European Summer Time      Europe
  tctx_tz_CET,                      //   1    Central European Time             Europe
  tctx_tz_CST,                      //  -6    Central Standard Time             North America
  tctx_tz_CXT,                      //   7    Christmas Island Time             Australia
  tctx_tz_D,                        //   4    Delta Time Zone                  <Military>
  tctx_tz_E,                        //   5    Echo Time Zone                   <Military>
  tctx_tz_EDT,                      //  -4    Eastern Daylight Time             North America
  tctx_tz_EEST,                     //   3    Eastern European Summer Time      Europe
  tctx_tz_EET,                      //   2    Eastern European Time             Europe
  tctx_tz_EST,                      //  -5    Eastern Standard Time             North America
  tctx_tz_F,                        //   6    Foxtrot Time Zone                <Military>
  tctx_tz_G,                        //   7    Golf Time Zone                   <Military>
  tctx_tz_H,                        //   8    Hotel Time Zone                  <Military>
  tctx_tz_HAA,                      //  -3    Heure Avancee de l'Atlantique     North America
  tctx_tz_HAC,                      //  -5    Heure Avancee du Centre           North America
  tctx_tz_HADT,                     //  -9    Hawaii-Aleutian Daylight Time     North America
  tctx_tz_HAE,                      //  -4    Heure Avancee de l'Est            North America
  tctx_tz_HAP,                      //  -7    Heure Avancee du Pacifique        North America
  tctx_tz_HAR,                      //  -6    Heure Avancee des Rocheuses       North America
  tctx_tz_HAST,                     // -10    Hawaii-Aleutian Standard Time     North America
  tctx_tz_HAT,                      //  -1.5  Heure Avancee de Terre-Neuve      North America
  tctx_tz_HAY,                      //  -8    Heure Avancee du Yukon            North America
  tctx_tz_HNA,                      //  -4    Heure Normale de l'Atlantique     North America
  tctx_tz_HNC,                      //  -6    Heure Normale du Centre           North America
  tctx_tz_HNE,                      //  -5    Heure Normale de l'Est            North America
  tctx_tz_HNP,                      //  -8    Heure Normale du Pacifique        North America
  tctx_tz_HNR,                      //  -7    Heure Normale des Rocheuses       North America
  tctx_tz_HNT,                      //  -2.5  Heure Normale de Terre-Neuve      North America
  tctx_tz_HNY,                      //  -9    Heure Normale du Yukon            North America
  tctx_tz_I,                        //   9    India Time Zone                  <Military>
  tctx_tz_IST,                      //   1    Irish Summer Time                 Europe
  tctx_tz_K,                        //  10    Kilo Time Zone                   <Military>
  tctx_tz_L,                        //  11    Lima Time Zone                   <Military>
  tctx_tz_M,                        //  12    Mike Time Zone                   <Military>
  tctx_tz_MDT,                      //  -6    Mountain Daylight Time            North America
  tctx_tz_MESZ,                     //   2    Mitteleuropaeische Sommerzeit      Europe
  tctx_tz_MEZ,                      //   1    Mitteleuropaeische Zeit            Europe
  tctx_tz_MST,                      //  -7    Mountain Standard Time            North America
  tctx_tz_N,                        //  -1    November Time Zone               <Military>
  tctx_tz_NDT,                      //  -1.5  Newfoundland Daylight Time        North America
  tctx_tz_NFT,                      //  11.5  Norfolk (Island) Time             Australia
  tctx_tz_NST,                      //  -2.5  Newfoundland Standard Time        North America
  tctx_tz_O,                        //  -2    Oscar Time Zone                  <Military>
  tctx_tz_P,                        //  -3    Papa Time Zone                   <Military>
  tctx_tz_PDT,                      //  -7    Pacific Daylight Time             North America
  tctx_tz_PST,                      //  -8    Pacific Standard Time             North America
  tctx_tz_Q,                        //  -4    Quebec Time Zone                 <Military>
  tctx_tz_R,                        //  -5    Romeo Time Zone                  <Military>
  tctx_tz_S,                        //  -6    Sierra Time Zone                 <Military>
  tctx_tz_T,                        //  -7    Tango Time Zone                  <Military>
  tctx_tz_U,                        //  -8    Uniform Time Zone                <Military>

  tctx_tz_V,                        //  -9    Victor Time Zone                 <Military>
  tctx_tz_W,                        // -10    Whiskey Time Zone                <Military>
  tctx_tz_WEST,                     //   1    Western European Summer Time      Europe
  tctx_tz_WET,                      //   0    Western European Time             Europe
  tctx_tz_WST,                      //   8    Western Standard Time             Australia
  tctx_tz_X,                        // -11    X-ray Time Zone                  <Military>
  tctx_tz_Y,                        // -12    Yankee Time Zone                 <Military>
  tctx_tz_Z,                        //   0    Zulu Time Zone                   <Military>

  // size of enum
  tctx_numtimezones
} TTimeZones;



#ifdef TIMEZONES_INTERNAL

typedef struct {
          short wMonth;
          short wDayOfWeek;
          short wNth;      // nth occurance
          short wHour;
          short wMinute;
        } tbl_tChange;


typedef struct {
          const char* name;
          const char* olsonName;
          short       bias;
          short       biasDST;
          const char* ident;
          const char* dynYear;
          tbl_tChange dst;
          tbl_tChange std;
        } tbl_tz_entry;



const tbl_tz_entry tbl_tz[tctx_numtimezones] =
{
  { "unknown",           NULL,                         0,  0, "x",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   0
  { "system",            NULL,                         0,  0, "x",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   0
  { "UTC",               NULL,                         0,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   0

  { "Afghanistan",       "Asia/Kabul",               270,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   4.5
  { "AKST/AKDT",         "America/Anchorage",       -540, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -9
  { "AKST/AKDT",         "America/Anchorage",       -540, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -9
  { "AKST/AKDT",         "America/Anchorage",       -540, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -9
  { "HNY/NAY",           "America/Anchorage",       -540, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -9
  { "HNY/NAY",           "America/Anchorage",       -540, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -9
  { "HNY/NAY",           "America/Anchorage",       -540, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -9
  { "Alaskan",           "America/Anchorage",       -540, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -9
  { "Alaskan",           "America/Anchorage",       -540, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -9
  { "Alaskan",           "America/Anchorage",       -540, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -9
  { "Arab",              "Asia/Riyadh",              180,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3
  { "Arabian",           "Asia/Dubai",               240,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   4
  { "Arabic",            "Asia/Baghdad",             180, 60, " ",    "", {  4, 0,1, 3,0 },  { 10, 0,1, 4,0 } },  //   3
  { "AST/ADT",           "America/Halifax",         -240, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -4
  { "AST/ADT",           "America/Halifax",         -240, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -4
  { "AST/ADT",           "America/Halifax",         -240, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -4
  { "HNA/HAA",           "America/Halifax",         -240, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -4
  { "HNA/HAA",           "America/Halifax",         -240, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -4
  { "HNA/HAA",           "America/Halifax",         -240, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -4
  { "Atlantic",          "America/Halifax",         -240, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -4
  { "Atlantic",          "America/Halifax",         -240, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -4
  { "Atlantic",          "America/Halifax",         -240, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -4
  { "ART/ARST",          "America/Buenos_Aires",    -180, 60, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -3
  { "ART/ARST",          "America/Buenos_Aires",    -180, 60, " ","2006", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -3
  { "ART/ARST",          "America/Buenos_Aires",    -180, 60, " ","2007", { 12,-1,29,23,59}, {  1,-1,1, 0,0 } },  //  -3
  { "ART/ARST",          "America/Buenos_Aires",    -180, 60, " ","2008", { 10, 6,3,23,59},  {  3, 6,3,23,59} },  //  -3
  { "ART/ARST",          "America/Buenos_Aires",    -180, 60, " ","2009", { 10, 6,3,23,59},  {  3, 6,2,23,59} },  //  -3
  { "AUS_Central",       "Australia/Darwin",         570,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   9.5
  { "AUS_Eastern",       "Australia/Sydney",         600, 60, " ",    "", { 10, 0,5, 2,0 },  {  3, 0,5, 3,0 } },  //  10
  { "Azerbaijan",        "Asia/Baku",                240, 60, " ",    "", {  3, 0,5, 4,0 },  { 10, 0,5, 5,0 } },  //   4
  { "Azores",            "Atlantic/Azores",          -60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //  -1
  { "Canada_Central",    "America/Regina",          -360,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "Cape_Verde",        "Atlantic/Cape_Verde",      -60,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -1
  { "Caucasus",          "Asia/Tbilisi",             240, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   4
  { "ACST/ACDT",         "Australia/Adelaide",       570, 60, " ",    "", { 10, 0,5, 2,0 },  {  3, 0,5, 3,0 } },  //   9.5
  { "Central_Australia", "Australia/Adelaide",       570, 60, " ",    "", { 10, 0,5, 2,0 },  {  3, 0,5, 3,0 } },  //   9.5
  { "Central_America",   "America/Guatemala",       -360,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "Central_Asia",      "Asia/Dhaka",               360,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   6
  { "Central_Brazilian", "America/Manaus",          -240, 60, " ",    "", { 11, 0,1, 0,0 },  {  2, 0,5, 0,0 } },  //  -4
  { "Central_Brazilian", "America/Manaus",          -240, 60, " ","2006", { 11, 0,1, 0,0 },  {  2, 0,2, 2,0 } },  //  -4
  { "Central_Brazilian", "America/Manaus",          -240, 60, " ","2007", { 11, 0,1, 0,0 },  {  2, 0,5, 0,0 } },  //  -4
  { "CET/CEST",          "Europe/Zurich",             60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "MEZ/MESZ",          "Europe/Berlin",             60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "Central_Europe",    "Europe/Budapest",           60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "Central_European",  "Europe/Warsaw",             60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "Central_Pacific",   "Pacific/Guadalcanal",      660,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  11
  { "CST/CDT",           "America/Chicago",         -360, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -6
  { "CST/CDT",           "America/Chicago",         -360, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -6
  { "CST/CDT",           "America/Chicago",         -360, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -6
  { "HNC/HAC",           "America/Chicago",         -360, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -6
  { "HNC/HAC",           "America/Chicago",         -360, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -6
  { "HNC/HAC",           "America/Chicago",         -360, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -6
  { "Central",           "America/Chicago",         -360, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -6
  { "Central",           "America/Chicago",         -360, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -6
  { "Central",           "America/Chicago",         -360, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -6
  { "Central_Mexico",    "America/Mexico_City",     -360, 60, " ",    "", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -6
  { "China",             "Asia/Shanghai",            480,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "Dateline",          "Etc/GMT+12",              -720,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -12
  { "East_Africa",       "Africa/Nairobi",           180,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3
  { "AEST/AEDT",         "Australia/Brisbane",       600,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  10
  { "East_Australia",    "Australia/Brisbane",       600,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  10
  { "EET/EEST",          "Europe/Minsk",             120, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   2
  { "East_Europe",       "Europe/Minsk",             120, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   2
  { "East_South_America","America/Sao_Paulo",       -180, 60, " ",    "", { 11, 0,1, 0,0 },  {  2, 0,5, 0,0 } },  //  -3
  { "East_South_America","America/Sao_Paulo",       -180, 60, " ","2006", { 11, 0,1, 0,0 },  {  2, 0,2, 2,0 } },  //  -3
  { "East_South_America","America/Sao_Paulo",       -180, 60, " ","2007", { 11, 0,1, 0,0 },  {  2, 0,5, 0,0 } },  //  -3
  { "EST/EDT",           "America/New_York",        -300, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -5
  { "EST/EDT",           "America/New_York",        -300, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -5
  { "EST/EDT",           "America/New_York",        -300, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -5
  { "HNE/HAE",           "America/New_York",        -300, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -5
  { "HNE/HAE",           "America/New_York",        -300, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -5
  { "HNE/HAE",           "America/New_York",        -300, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -5
  { "Eastern",           "America/New_York",        -300, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -5
  { "Eastern",           "America/New_York",        -300, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -5
  { "Eastern",           "America/New_York",        -300, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -5
  { "Egypt",             "Africa/Cairo",             120, 60, " ",    "", {  4, 4,5,23,59 }, {  9, 4,5,23,59 }},  //   2
  { "Ekaterinburg",      "Asia/Yekaterinburg",       300, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   5
  { "Fiji",              "Pacific/Fiji",             720,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  12
  { "FLE",               "Europe/Kiev",              120, 60, " ",    "", {  3, 0,5, 3,0 },  { 10, 0,5, 4,0 } },  //   2
  { "Georgian",          "Etc/GMT-3",                180,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3
  { "GMT",               "Europe/London",              0, 60, " ",    "", {  3, 0,5, 1,0 },  { 10, 0,5, 2,0 } },  //   0
  { "Greenland",         "America/Godthab",         -180, 60, " ",    "", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -3
  { "Greenwich",         "Africa/Casablanca",          0,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   0
  { "GTB",               "Europe/Istanbul",          120, 60, " ",    "", {  3, 0,5, 3,0 },  { 10, 0,5, 4,0 } },  //   2
  { "HST/HPT",           "Pacific/Honolulu",        -600,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -10
  { "HAST/HADT",         "Pacific/Honolulu",        -600,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -10
  { "Hawaiian",          "Pacific/Honolulu",        -600,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -10
  { "India",             "Asia/Calcutta",            330,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   5.5
  { "Iran",              "Asia/Tehran",              210,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3.5
  { "Iran",              "Asia/Tehran",              210, 60, " ","2005", {  3, 0,1, 2,0 },  {  9, 2,4, 2,0 } },  //   3.5
  { "Iran",              "Asia/Tehran",              210,  0, " ","2006", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3.5
  { "Israel",            "Asia/Jerusalem",           120, 60, " ",    "", {  3, 5,5, 2,0 },  {  9, 0,3, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120,  0, " ","2004", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2005", {  4,-1,1, 2,0 },  { 10,-1,9, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2006", {  3,-1,31,2,0 },  { 10,-1,1, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2007", {  3,-1,30,2,0 },  {  9,-1,16,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2008", {  3,-1,28,2,0 },  { 10,-1,5, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2009", {  3,-1,27,2,0 },  {  9,-1,27,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2010", {  3,-1,26,2,0 },  {  9,-1,12,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2011", {  4,-1,1, 2,0 },  { 10,-1,2, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2012", {  3,-1,30,2,0 },  {  9,-1,23,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2013", {  3,-1,29,2,0 },  {  9,-1,8, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2014", {  3,-1,28,2,0 },  {  9,-1,28,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2015", {  3,-1,27,2,0 },  {  9,-1,20,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2016", {  4,-1,1, 2,0 },  { 10,-1,9, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2017", {  3,-1,31,2,0 },  {  9,-1,24,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2018", {  3,-1,30,2,0 },  {  9,-1,16,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2019", {  3,-1,29,2,0 },  { 10,-1,6, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2020", {  3,-1,27,2,0 },  {  9,-1,27,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2021", {  3,-1,26,2,0 },  {  9,-1,12,2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120, 60, " ","2022", {  4,-1,1, 2,0 },  { 10,-1,2, 2,0 } },  //   2
  { "Israel",            "Asia/Jerusalem",           120,  0, " ","2023", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "Jordan",            "Asia/Amman",               120, 60, " ",    "", {  3, 4,5, 0,0 },  {  9, 5,5, 1,0 } },  //   2
  { "Korea",             "Asia/Seoul",               540,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   9
  { "Mexico",            "America/Mexico_City",     -360, 60, " ",    "", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -6
  { "Mexico_2",          "America/Chihuahua",       -420, 60, " ",    "", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -7
  { "Mid_Atlantic",      "Atlantic/South_Georgia",  -120, 60, " ",    "", {  3, 0,5, 2,0 },  {  9, 0,5, 2,0 } },  //  -2
  { "Middle_East",       "Asia/Beirut",              120, 60, " ",    "", {  3, 0,5, 0,0 },  { 10, 6,5,23,59 }},  //   2
  { "Montevideo",        "America/Montevideo",      -180, 60, " ",    "", { 10, 0,1, 2,0 },  {  3, 0,2, 2,0 } },  //  -3
  { "MST/MDT",           "America/Denver",          -420, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -7
  { "MST/MDT",           "America/Denver",          -420, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -7
  { "MST/MDT",           "America/Denver",          -420, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -7
  { "HNR/HAR",           "America/Denver",          -420, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -7
  { "HNR/HAR",           "America/Denver",          -420, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -7
  { "HNR/HAR",           "America/Denver",          -420, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -7
  { "Mountain",          "America/Denver",          -420, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -7
  { "Mountain",          "America/Denver",          -420, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -7
  { "Mountain",          "America/Denver",          -420, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -7
  { "Mountain_Mexico",   "America/Chihuahua",       -420, 60, " ",    "", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -7
  { "Myanmar",           "Asia/Rangoon",             390,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   6.5
  { "North_Central_Asia","Asia/Novosibirsk",         360, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   6
  { "Namibia",           "Africa/Windhoek",          120,-60, " ",    "", {  4, 0,1, 2,0 },  {  9, 0,1, 2,0 } },  //   2
  { "Nepal",             "Asia/Katmandu",            345,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   5.75
  { "New_Zealand",       "Pacific/Auckland",         720, 60, " ",    "", { 10, 0,1, 2,0 },  {  3, 0,3, 3,0 } },  //  12
  { "NST/NDT",           "America/St_Johns",        -210, 60, " ",    "", {  3, 0,2, 0,1 },  { 11, 0,1, 0,1 } },  //  -3.5
  { "NST/NDT",           "America/St_Johns",        -210, 60, " ","2006", {  4, 0,1, 0,1 },  { 10, 0,5, 0,0 } },  //  -3.5
  { "NST/NDT",           "America/St_Johns",        -210, 60, " ","2007", {  3, 0,2, 0,1 },  { 11, 0,1, 0,0 } },  //  -3.5
  { "HNT/HAT",           "America/St_Johns",        -210, 60, " ",    "", {  3, 0,2, 0,1 },  { 11, 0,1, 0,1 } },  //  -3.5
  { "HNT/HAT",           "America/St_Johns",        -210, 60, " ","2006", {  4, 0,1, 0,1 },  { 10, 0,5, 0,0 } },  //  -3.5
  { "HNT/HAT",           "America/St_Johns",        -210, 60, " ","2007", {  3, 0,2, 0,1 },  { 11, 0,1, 0,0 } },  //  -3.5
  { "Newfoundland",      "America/St_Johns",        -210, 60, " ",    "", {  3, 0,2, 0,1 },  { 11, 0,1, 0,1 } },  //  -3.5
  { "Newfoundland",      "America/St_Johns",        -210, 60, " ","2006", {  4, 0,1, 0,1 },  { 10, 0,5, 0,0 } },  //  -3.5
  { "Newfoundland",      "America/St_Johns",        -210, 60, " ","2007", {  3, 0,2, 0,1 },  { 11, 0,1, 0,0 } },  //  -3.5
  { "North_Asia_East",   "Asia/Irkutsk",             480, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   8
  { "North_Asia",        "Asia/Krasnoyarsk",         420, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   7
  { "Pacific_SA",        "America/Santiago",        -240, 60, " ",    "", { 10, 6,2,23,59 }, {  3, 6,2,23,59 }},  //  -4
  { "PST/PDT",           "America/Los_Angeles",     -480, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -8
  { "PST/PDT",           "America/Los_Angeles",     -480, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -8
  { "PST/PDT",           "America/Los_Angeles",     -480, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -8
  { "HNP/HAP",           "America/Los_Angeles",     -480, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -8
  { "HNP/HAP",           "America/Los_Angeles",     -480, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -8
  { "HNP/HAP",           "America/Los_Angeles",     -480, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -8
  { "Pacific",           "America/Los_Angeles",     -480, 60, " ",    "", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -8
  { "Pacific",           "America/Los_Angeles",     -480, 60, " ","2006", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -8
  { "Pacific",           "America/Los_Angeles",     -480, 60, " ","2007", {  3, 0,2, 2,0 },  { 11, 0,1, 2,0 } },  //  -8
  { "Pacific_Mexico",    "America/Tijuana",         -480, 60, " ",    "", {  4, 0,1, 2,0 },  { 10, 0,5, 2,0 } },  //  -8
  { "Romance",           "Europe/Paris",              60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "Russian",           "Europe/Moscow",            180, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   3
  { "SA_Eastern",        "Etc/GMT+3",               -180,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -3
  { "SA_Pacific",        "America/Bogota",          -300,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "SA_Western",        "America/La_Paz",          -240,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -4
  { "Samoa",             "Pacific/Apia",            -660,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -11
  { "SE_Asia",           "Asia/Bangkok",             420,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   7
  { "Singapore",         "Asia/Singapore",           480,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "South_Africa",      "Africa/Johannesburg",      120,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "Sri_Lanka",         "Asia/Colombo",             330,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   5.5
  { "Taipei",            "Asia/Taipei",              480,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "Tasmania",          "Australia/Hobart",         600, 60, " ",    "", { 10, 0,1, 2,0 },  {  3, 0,5, 3,0 } },  //  10
  { "Tokyo",             "Asia/Tokyo",               540,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   9
  { "Tonga",             "Pacific/Tongatapu",        780,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  13
  { "US_Eastern",        "Etc/GMT+5",               -300,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "US_Mountain",       "America/Phoenix",         -420,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -7
  { "Vladivostok",       "Asia/Vladivostok",         600, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //  10
  { "West_Australia",    "Australia/Perth",          480, 60, " ",    "", { 10, 0,5, 2,0 },  {  3, 0,5, 3,0 } },  //   8
  { "West_Australia",    "Australia/Perth",          480,  0, " ","2005", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "West_Australia",    "Australia/Perth",          480, 60, " ","2006", { 12,-1,1, 2,0 },  {  1,-1,1, 0,0 } },  //   8
  { "West_Australia",    "Australia/Perth",          480, 60, " ","2007", { 10, 0,5, 2,0 },  {  3, 0,5, 3,0 } },  //   8
  { "West_Central_Africa","Africa/Lagos",             60,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "WET/WEST",          "Europe/Berlin",             60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "West_Europe",       "Europe/Berlin",             60, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   1
  { "West_Asia",         "Asia/Karachi",             300,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   5
  { "West_Pacific",      "Pacific/Port_Moresby",     600,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  10
  { "Yakutsk",           "Asia/Yakutsk",             540, 60, " ",    "", {  3, 0,5, 2,0 },  { 10, 0,5, 3,0 } },  //   9

  { "A",                 NULL,                        60,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "ACDT",              NULL,                       630,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  10.5
  { "ACST",              NULL,                       570,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   9.5
  { "ADT",               NULL,                      -180,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -3
  { "AEDT",              NULL,                       660,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  11
  { "AEST",              NULL,                       600,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  10
  { "AKDT",              NULL,                      -480,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -8
  { "AKST",              NULL,                      -540,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -9
  { "AST",               NULL,                      -240,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -4
  { "AWST",              NULL,                       480,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "B",                 NULL,                       120,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "BST",               NULL,                        60,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "C",                 NULL,                       180,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3
  { "CDT",               NULL,                      -300,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "CEST",              NULL,                       120,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "CET",               NULL,                        60,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "CST",               NULL,                      -360,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "CXT",               NULL,                       420,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   7
  { "D",                 NULL,                       240,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   4
  { "E",                 NULL,                       300,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   5
  { "EDT",               NULL,                      -240,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -4
  { "EEST",              NULL,                       180,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   3
  { "EET",               NULL,                       120,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "EST",               NULL,                      -300,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "F",                 NULL,                       360,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   6
  { "G",                 NULL,                       420,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   7
  { "H",                 NULL,                       480,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "HAA",               NULL,                      -180,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -3
  { "HAC",               NULL,                      -300,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "HADT",              NULL,                      -540,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -9
  { "HAE",               NULL,                      -240,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -4
  { "HAP",               NULL,                      -420,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -7
  { "HAR",               NULL,                      -360,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "HAST",              NULL,                      -600,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -10
  { "HAT",               NULL,                       -90,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -1.5
  { "HAY",               NULL,                      -480,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -8
  { "HNA",               NULL,                      -240,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -4
  { "HNC",               NULL,                      -360,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "HNE",               NULL,                      -300,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "HNP",               NULL,                      -480,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -8
  { "HNR",               NULL,                      -420,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -7
  { "HNT",               NULL,                      -150,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -2.5
  { "HNY",               NULL,                      -540,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -9
  { "I",                 NULL,                       540,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   9
  { "IST",               NULL,                        60,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "K",                 NULL,                       600,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  10
  { "L",                 NULL,                       660,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  11
  { "M",                 NULL,                       720,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  12
  { "MDT",               NULL,                      -360,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "MESZ",              NULL,                       120,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   2
  { "MEZ",               NULL,                        60,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "MST",               NULL,                      -420,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -7
  { "N",                 NULL,                       -60,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -1
  { "NDT",               NULL,                       -90,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -1.5
  { "NFT",               NULL,                       690,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  11.5
  { "NST",               NULL,                      -150,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -2.5
  { "O",                 NULL,                      -120,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -2
  { "P",                 NULL,                      -180,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -3
  { "PDT",               NULL,                      -420,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -7
  { "PST",               NULL,                      -480,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -8
  { "Q",                 NULL,                      -240,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -4
  { "R",                 NULL,                      -300,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -5
  { "S",                 NULL,                      -360,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -6
  { "T",                 NULL,                      -420,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -7
  { "U",                 NULL,                      -480,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -8

  { "V",                 NULL,                      -540,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //  -9
  { "W",                 NULL,                      -600,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -10
  { "WEST",              NULL,                        60,  0, "d",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   1
  { "WET",               NULL,                         0,  0, "s",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   0
  { "WST",               NULL,                       480,  0, " ",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   8
  { "X",                 NULL,                      -660,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -11
  { "Y",                 NULL,                      -720,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  // -12
  { "Z",                 NULL,                         0,  0, "m",    "", {  0, 0,0, 0,0 },  {  0, 0,0, 0,0 } },  //   0
};

#endif // TIMEZONES_INTERNAL

#endif // TZ_TABLE_H
/* eof */
