/* 
 * Copyright (c) 2006-2011, Guillaume Gimenez <guillaume@blackmilk.fr>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of G.Gimenez nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL G.Gimenez SA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *     * Guillaume Gimenez <guillaume@blackmilk.fr>
 *
 */
#include <raii.H>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <cmath>

#include "image.hpp"
#include "exif.hpp"

/*
#define ROOT "/home/guigui/photos"
#define TMP "/var/raii/gallery"
*/

using std::ifstream;
using namespace raii;
using namespace raii::sql;

String path_encode(const String& str);
String getCurrentUser(HttpServletRequest& request);
bool isAdmin(const String& user);
void hitImage(const String& what, const String& path);
bool isPathInfoPublic(HttpServletRequest& request, HttpServletResponse& response);
bool isPathInfoHidden(const String& user, HttpServletRequest& request, HttpServletResponse& response);
String getVPhotoRealPath(const String& pathInfo);
void securityCheck(HttpServlet* servlet, HttpServletRequest& request, HttpServletResponse& response);



class Link {
        public:
        String title, filename, name;
        Link(const String& _title, const String& _filename, const String& _name)
        : title(_title), filename(_filename), name(_name) {}
};

class IndexAttribute : public Object {
  public:
    StringStream dirs;
    StringStream files;
    Vector<Link> links;
    IndexAttribute()
        : Object(), dirs(), files()
    {}
    ~IndexAttribute(){ }
};

class IndexCache : public raii::Object {
        Map<String, ptr<IndexAttribute> > entry;
        Mutex mutex;
        public:
        ptr<IndexAttribute> getEntry(const String& key) {
                Lock l(mutex);
                return entry[key];
        }
        void setEntry(const String& key, ptr<IndexAttribute> attr) {
                entry[key]=attr;
        }
};


class ImageDesc : public raii::Object {
  public:
  int id;
  String alias;
  String author;
  String description;
  int preview;
  int download;
  bool isPublic;
  bool isHidden;
  String owner;
  bool isVDir;
  ImageDesc(const String& pathInfo)
    : id(0),
    alias(),
    author(),
    description(),
    preview(0),
    download(0),
    isPublic(false),
    isHidden(false),
    owner(""),
    isVDir(false)
    {
      Connection conn;
      String req=String("SELECT * FROM gallery_entry WHERE created_at is NULL AND path='")
                             + ( path_encode(getVPhotoRealPath(url_decode(pathInfo))) )+ String("'");
      ResultSet rs = conn.query(req);
      // description
      if (  rs.rowCount() != 0 ) {
        rs.next();
        alias=url_decode(rs["alias"]);
        author=url_decode(rs["author"]);
        id=atoi(rs["id"].c_str());
        description=url_decode(rs["description"]);
        isPublic=(rs["secured"]=="false");
        isHidden=(rs["secured"]=="hidden");
      }

      //vdir owner
      String pathpart=pathInfo.substr(0,pathInfo.rfind("/",pathInfo.length()-2))+"/";
      String filepart=pathInfo.substr(pathInfo.rfind("/",pathInfo.length()-2)+1);
      rs=conn.query(String("SELECT * FROM gallery_vdir WHERE path='")+path_encode(url_decode(pathpart))+"' AND name ='"+path_encode(url_decode(filepart))+"'");
      if ( rs.next() ) {
              owner=rs["owner"];
              isVDir=true;
      }
      
      // stats
      String vpath=pathInfo.substr(0,pathInfo.rfind("/"))+"/";
      String vname=pathInfo.substr(pathInfo.rfind("/")+1);
      rs = conn.query(String("SELECT * FROM gallery_vphoto WHERE dir='")+vpath+"' AND name='"+vname+"'");
      String rpath;
      if ( rs.next() ) {
              rpath=rs["realpath"];
      }
      else {
              rpath=pathInfo;
      }
      rs = conn.query(String("SELECT * FROM gallery_stats WHERE path = '") + rpath + String("'"));
      if ( rs.rowCount() != 0 ) {
        rs.next();
        preview=atoi(rs["preview"].c_str());
        download=atoi(rs["download"].c_str());
      }
    }
};

class Dir : public raii::Object {
        
        
  static const char *months[];
  
  public:
  
  
  Dir(const String& root, const String& pathInfo, const String& dirname,int timestamp=0) {
          
    ImageDesc imgdsc(path_encode(pathInfo+dirname+String("/")));

    struct tm bdtime;
    struct stat st;

    if ( !timestamp ) {
            if ( stat( (root+pathInfo+dirname).c_str(), &st) < 0 )
                    throw IOException(dirname.c_str());
    }
    else {
            st.st_mtime=timestamp;
    }
    
    if ( !localtime_r(&st.st_mtime,&bdtime) )
            throw IOException("localtime");
    
    mtime=st.st_mtime;
    year=bdtime.tm_year+1900;
    hrmonth=months[bdtime.tm_mon];
    month=bdtime.tm_mon+1;
    day=bdtime.tm_mday;
    href=dirname;
    a=imgdsc.alias.empty()?dirname:imgdsc.alias;
    desc=imgdsc.description.empty()?String(""):imgdsc.description;
    isPublic=imgdsc.isPublic;
    isHidden=imgdsc.isHidden;
    owner=imgdsc.owner;
    isVDir=imgdsc.isVDir;
  };
  
  int mtime;
  int year;
  int month;
  String hrmonth;
  int day;
  String a;
  String href;
  String desc;
  bool isPublic;
  bool isHidden;
  String owner;
  bool isVDir;
};

bool sortDir(const Dir& a, const Dir& b);
bool sendToPhotoService(const String& filename, const String& user, const String& password);
bool isVDir(const String& pathInfo);
bool isVDirOwnedBy(const String& pathInfo,const String& user);
String getVDirPathPart(const String& pathInfo);
String getVDirDirPart(const String& pathInfo);
Map<String,String> getVDirsForUser(const String& user);
void updateDirDate(String root, String pathInfo, int year, int month, int day);








//#include <iostream>
//#include <iomanip>
//#include <cassert>

class ExifTags : public raii::Object {
        public:
        raii::String Image_DateTime;  //Ascii
        raii::String Image_Make;      //Ascii
        raii::String Image_Model;     //Ascii
        double Photo_ExposureTime;      //Rational
        double Photo_FNumber;           //Rational
        long Photo_ISOSpeedRatings;   //Short
        long Photo_Orientation; //Short
        raii::String Photo_FocalLength;       //Rational
        long Photo_Flash;             //Short
        raii::String Photo_FocalLengthIn35mmFilm; //Rational
	raii::String Nikon3_Lens;                     //Rational[4]
  
        double Ev,Av,Tv,Bv,Sv,B,I;
	ExifTags() : Image_DateTime("Unknown"), Image_Make("Unknown"), Image_Model("Unknown"),
			Photo_ExposureTime(0), Photo_FNumber(0), Photo_ISOSpeedRatings(-1),
			Photo_FocalLength(""), Photo_Flash(0), Photo_FocalLengthIn35mmFilm(""),
			Nikon3_Lens(""),Ev(0),Av(0),Tv(0),Bv(0),Sv(0),B(0),I(0) {}
        
        ExifTags(const raii::String& filename) {
                Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(filename.c_str());
                if ( image.get() == 0 )
                        throw "get";
                image->readMetadata();
                
                Exiv2::ExifData &exifData = image->exifData();
                if (exifData.empty()) {
                        raii::String error(filename);
                        error += ": No Exif data found in the file";
                        throw Exiv2::Error(1, error);
                }

		/*
		 * debug
		Logger l("exif");
		Exiv2::ExifData::const_iterator end = exifData.end();
		for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end ; ++i) {
			StringStream ss;
			ss << i->key() << ": (" << i->typeName() << ")" << i->value();
			l(ss.str());
		}
		*/


                Image_DateTime=exifData["Exif.Photo.DateTimeOriginal"].toString();
                if ( Image_DateTime.empty() )
                        Image_DateTime=exifData["Exif.Image.DateTime"].toString();
                Image_Make=exifData["Exif.Image.Make"].toString();
                Image_Model=exifData["Exif.Image.Model"].toString();
                Photo_ExposureTime=exifData["Exif.Photo.ExposureTime"].toFloat();
                Photo_FNumber=exifData["Exif.Photo.FNumber"].toFloat();

                Photo_ISOSpeedRatings=exifData["Exif.Photo.ISOSpeedRatings"].toLong();
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Photo.ExposureIndex"].toLong();

		//Nikon hispeed
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Nikon3.ISOSettings"].toLong(1);

		//Nikon other
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Nikon3.ISOSpeed"].toLong();
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Nikon2.ISOSpeed"].toLong();
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Nikon1.ISOSpeed"].toLong();
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Nikonli.ISOExpansion"].toLong();
		if ( Photo_ISOSpeedRatings <= 0 )
		  Photo_ISOSpeedRatings=exifData["Exif.Nikonli.ISOExpansion2"].toLong();


                Photo_FocalLength=ftostring(exifData["Exif.Photo.FocalLength"].toFloat());
                Photo_Flash=exifData["Exif.Photo.Flash"].toLong();
                Photo_FocalLengthIn35mmFilm=ftostring(exifData["Exif.Photo.FocalLengthIn35mmFilm"].toFloat());

		Photo_Orientation = exifData["Exif.Image.Orientation"].toLong();
		
		computeAPEX();
                

		long Nikon3_LensType=exifData["Exif.Nikon3.LensType"].toLong();
/*
 * 01 0001  AI-s             manual
 * 00 0000  AF	             non-D
 * 02 0010  AF-(D/S)         50/1.4 85/1.8 28-200/3.5-5.6
 * 06 0110  AF-(D/S) G       30/1.4 12-24/4 18-70/3.5-4.5
 * 10 1010  AF-(D/S) VR      j'en ai pas
 * 14 1110  AF-(D/S) G VR    16-85
 *       _
 *    VGDA
 *    R  F
 */
		switch(Nikon3_LensType) {
			case -1:
				return;
			case 0:
				Nikon3_Lens="AF "; // non D
				break;
			case 1:  //manual
				Nikon3_Lens="AI-s ";
				break;
			case 2: // cas des 85/1.8 50/1.4 et 28-200/3.3-5.6
				Nikon3_Lens="AF-D ";
				break;
			case 6: // cas des 18-70/3.5-4.5 30/1.4 12-24/4
				Nikon3_Lens="AF-S G ";
				break;
			case 10:
				Nikon3_Lens="AF-D VR ";
				break;
			case 14: //cas du 16-85/3.5-4.5
				Nikon3_Lens="AF-S G VR ";
				break;
			default:
				Nikon3_Lens="??? ";
		}
		double fw,ft,aw,at;
		fw=exifData["Exif.Nikon3.Lens"].toFloat(0);
		if ( fw < 0 ) return;
		ft=exifData["Exif.Nikon3.Lens"].toFloat(1);
		aw=exifData["Exif.Nikon3.Lens"].toFloat(2);
		at=exifData["Exif.Nikon3.Lens"].toFloat(3);
		if ( fw == ft )
			Nikon3_Lens+= ftostring(fw)+"mm ";
		else
			Nikon3_Lens+= ftostring(fw)+"-"+ftostring(ft)+"mm ";
		if ( aw == at )
			Nikon3_Lens+= raii::String("f/")+ftostring(aw);
		else
			Nikon3_Lens+= raii::String("f/")+ftostring(aw)+"-"+ftostring(at);
        }

	void computeAPEX() {
                Av = 2 * log (Photo_FNumber) / log (2);
                Tv = - log (Photo_ExposureTime) / log (2);
                Sv = log (Photo_ISOSpeedRatings/3.125) / log (2);
                Ev = Av + Tv;
                Bv = Ev - Sv;
                //Bv = log(B / (0.3*11.4) )/log(2);
                //cd/m²
                B = exp(Bv*log(2)) * (M_1_PI) * 11.4;
		//lux selon APEX.pdf ( à 16% )
		//I = exp(Bv*log(2)) * (M_1_PI) * 224.;
		// selon moi
		I = B / ( M_1_PI * .18 );
	}
};




static const char *type[]={"processed","preview","thumbnail",NULL};
static const int maxsize[]={0,721,129};
#define T_PROCESSED 0
#define T_PREVIEW 1
#define T_THUMBNAIL 2
#define T_LAST 3


static inline String getMimeType(const String& filename) {

                request_rec *rr=ap_sub_req_lookup_file(
                                        filename.c_str(),
                                        apacheRequest,
                                        NULL);
                if ( !rr || !rr->content_type )
                    throw ServletException((String("Unknown mimeType for ")+filename).c_str());
                String ct(rr->content_type);
                ap_destroy_sub_req(rr);
                return ct;
        }

static inline bool isRaw(const String& filename) {

                if ( getMimeType(filename) == "image/x-raw" )
                    return true;
                return false;
        }

static inline bool isVideo(const String& filename) {

                if ( getMimeType(filename).substr(0,5) == "video" )
                    return true;
                return false;
        }

static inline void prepareTmpDir(const String& root, const String& tmp, const String& filename) {


                for ( int i=0 ; type[i] != NULL ; ++ i ) {
                        size_t pos=0;
                        int ret;
                        struct stat st;
                        String thumbnail=tmp + String("/") + type[i] + root + filename + String(".jpg");

                        while ( (pos=thumbnail.find("/",pos+1) ) <= thumbnail.length() ) {
                                String path = thumbnail.substr(0,pos);
                                ret=stat( path.c_str(),&st);

                                if ( ret != 0 || ! S_ISDIR(st.st_mode)  )
                                        if ( mkdir(path.c_str(),0755) != 0 && errno != EEXIST )
                                                throw IOException((String("Impossible de créer le répertoire [") + path + "]").c_str() );
                        }
                }
        }
static inline String getRealPath(const String& path_info) {

                String pathpart=url_decode(path_info).substr(0,path_info.rfind("/")) + "/";
                String namepart=url_decode(path_info).substr(path_info.rfind("/")+1);
                String realpath;
                {
                        Connection conn;
                        String req=String("SELECT * FROM gallery_vphoto WHERE dir='")+path_encode(pathpart)+"' AND name='"+path_encode(namepart)+"'";
                        ResultSet rs=conn.query(req);
                        if ( rs.next() )
                                realpath=rs["realpath"];
                        else
                                realpath=path_info;
                }
                return realpath;
        }





#include "html.H"
#include "ImageEditor.H"
#include "Image.H"
//
