#include <string>
#include <iostream>

#include <Magick++.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstring>
#include <unistd.h>
#include <cstdio>

using namespace std;
using namespace Magick;


        bool isThumbnailUpToDate(const string& type, int maxsize, const string& filename) {

                struct stat stImage,stThumbnail;
                string thumbnail=getenv("GALLERY_TMP") + string("/") + type + filename + string(".jpg");
                int ret;

                stat(filename.c_str(),&stImage);
                ret=stat(thumbnail.c_str(),&stThumbnail);

                if ( ret < 0 || ( stImage.st_mtime > stThumbnail.st_mtime ) )
                        return false;
                return true;
        }


        void makeThumbnail(const string& type, int maxsize, const string& filename) {

		++maxsize;

                Geometry geo;
                int w,h,W,H;
                double coef;
                string thumbnail=getenv("GALLERY_TMP") + string("/") + type + filename + string(".jpg");

                Image image;
                try {
                        image.read( filename );
                }
                catch (...) {
                        throw filename.c_str();
                }
                geo=image.size();
                w=geo.width();
                h=geo.height();
                if ( h == 0 || w == 0)
                        throw "L'image a une taille nulle.";

                if ( w < h )
                        coef=((double)maxsize)/((double)h);
                else
                        coef=((double)maxsize)/((double)w);

                W=(int)(w*coef);
                H=(int)(h*coef);

                image.scale(Geometry(W,H));
		const double kernel[]={0,-1,0,-1,8,-1,0,-1,0};
		image.convolve(3,kernel);
                try {
                        image.write( thumbnail );
                }
                catch (...) {
                        throw thumbnail.c_str();
                }

        }


        void prepareTmpDir(const string& type, int maxsize, const string& filename) {

                int pos=0;
                int ret;
                struct stat st;
                string thumbnail=getenv("GALLERY_TMP") + string("/") + type + filename + string(".jpg");

                while ( (unsigned)(pos=thumbnail.find("/",pos+1) ) <= thumbnail.length() ) {
                        string path = thumbnail.substr(0,pos);
                        ret=stat( path.c_str(),&st);

                        if ( ret != 0 || ! S_ISDIR(st.st_mode)  )
                                if ( mkdir(path.c_str(),0755) != 0 )
                                        throw (string("Imossible de créer le répertoire ") + path).c_str() ;
                }
        }


        bool prepareThumbnail(const string& type, int maxsize,const string& filename) {

                bool isUpToDate=isThumbnailUpToDate(type,maxsize,filename);
                if ( ! isUpToDate ) {
                        prepareTmpDir(type,maxsize,filename);
                        makeThumbnail(type,maxsize,filename);
                }
                return isUpToDate;
        }

void doThumbnail(const string& realpath,
		 const string& type,
		 int maxsize) {

                bool isUpToDate;
/*
                string filename = getenv("GALLERY_ROOT");
		       filename += realpath;
*/
		string filename=realpath;

                struct stat st;
                if ( stat(filename.c_str(),&st) == -1 ) {
                        throw "Not found";
		}

                isUpToDate=prepareThumbnail(type,maxsize,filename);
        }
int main( int argc, char **argv) {
	try {
		doThumbnail(argv[1],"thumbnail",128);
		doThumbnail(argv[1],"preview",720);
	} catch ( char const*e) {
		std::cerr << e << std::endl;
	}
	return 0;
}
