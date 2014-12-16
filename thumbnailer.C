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
		double kernel[]={0,-1,0,-1,8,-1,0,-1,0};
		double div = 0;
		int i;
		int order = 3;
		int n = order * order;
		for ( i = 0 ; i < n ; ++i )
		  div+=kernel[i];
		for ( i = 0 ; i < n ; ++i )
		  kernel[i]/=div;
		image.convolve(order, kernel);
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
