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
#include "common.H"

String convertRaw(const String& root, const String& tmp, const String& filename) {
  
  prepareTmpDir(root,tmp,filename);
  String tfile=tmp + String("/") + type[T_PROCESSED] + root + filename + String(".ppm");
  int rc = system((String("dcraw -w -c ")+root+filename+"> "+tfile).c_str());
  if ( rc ) {
    Logger log("converRaw");
    log("system call failed with rc="+itostring(rc));
  }
  return tfile;
}


void makeThumbnail(const String& root, const String& tmp, const String& filename) {


   Magick::Geometry geo;
   int w,h,W,H;
   double coef;
   
   Magick::Image image;
   
   bool israw=false;
   israw=isRaw(root+filename);
   
   /* si un traitement existe en base, il faut l'appliquer et rendre la main */ {
     ptr<Process> process = new Process( root,tmp,filename);
     Connection conn;
     bool processExists = false;
     if ( israw ) processExists = process->loadFrom(conn,"__default__");
     processExists = process->loadFrom(conn,filename) || processExists;
     
     if ( processExists ) {
       for ( int i = T_LAST-1 ; i >= 0 ; -- i ) {
        
	 String thumbnail=tmp + String("/") + type[i] + root + filename + String(".jpg");
                        
	 Magick::Blob blob;
	 process->getBlob(maxsize[i],&blob);
                                
	 FILE *f = fopen(thumbnail.c_str(),"w");
	 if ( !f ) throw IOException(("ecriture image "+thumbnail).c_str());
	 int rc = fwrite(blob.data(),blob.length(),1,f);
	 if ( !rc ) {
	   Logger log("makeThumbnail");
	   log(String("fwrite call failed on ")+thumbnail+": "+strerror(errno));
	 }
	 fclose(f);
       }
       return;
     }
   }
                
   try {
     if ( israw ) {

       String tfile = convertRaw(root,tmp,filename);
       image.read(tfile);
       unlink(tfile.c_str());
     }
     else
       {
	 image.read( root+filename );
	 try {
	   enum { UP_IS_UNDEF = 0, UP_IS_UP = 1 , UP_IS_LEFT = 2, UP_IS_BOTTOM = 4, UP_IS_RIGHT = 8};
	   ExifTags etags(root+filename);
	   switch (etags.Photo_Orientation)
	     {
	     case UP_IS_LEFT:
	       image.rotate(90.);
	     case UP_IS_RIGHT:
	       image.rotate(-90.);
	       break;
	     case UP_IS_BOTTOM:
	       image.rotate(180.);
	       break;
	     case UP_IS_UP:
	     default:
	       (void)0;
	     }
	 }
	 catch(...) {}
       }
   }
   catch (std::exception& e) {
     Logger log("magick++");
     log(e.what());
     throw ImageException((root+filename).c_str());
   }
   geo=image.size();
   w=geo.width();
   h=geo.height();

   if ( h == 0 || w == 0)
     throw ServletException("L'image a une taille nulle.");

   for ( int i = 0 ; i < T_LAST ; ++ i ) {

     String thumbnail=tmp + String("/") + type[i] + root + filename + String(".jpg");

     if ( maxsize[i] > 0 ) {
       if ( w < h )
	 coef=((double)maxsize[i])/((double)h);
       else
	 coef=((double)maxsize[i])/((double)w);
       W=(int)(w*coef);
       H=(int)(h*coef);

       image.scale(Magick::Geometry(W,H));
     }

     if ( ! israw && i == T_PROCESSED ) continue;

     Magick::Image tmpImage = image;

     double kernel[]={0,-1,0,-1,8,-1,0,-1,0};
     //const double kernel[]={0,1,0,1,-4,1,0,1,0};
     normalizeKernel(3, kernel);
     tmpImage.convolve(3,kernel);
     tmpImage.interlaceType(Magick::LineInterlace);
     tmpImage.quality(95);
     try {
       tmpImage.write( thumbnail );
     }
     catch (std::exception& e) {
       Logger log("magick++");
       log(e.what());
       throw IOException(thumbnail.c_str());
     }
   }

 }

int getProcessDate(const String& filename) {
  int ts=-1;
  Connection conn;
  ResultSet rs = conn.query("SELECT date FROM gallery_process_ts WHERE filename = '"+path_encode(filename)+"' ORDER BY date");
  while (rs.next()) ts=rs["date"].toi();
  return ts;
}

bool isThumbnailUpToDate(const String& root, const String& tmp, const String& filename) {
  
  struct stat stImage,stThumbnail;
  String thumbnail=tmp + String("/") + type[T_PREVIEW] + root + filename + String(".jpg");
  int ret;

  stat((root+filename).c_str(),&stImage);
  ret=stat(thumbnail.c_str(),&stThumbnail);

  if ( ret < 0 || ( stImage.st_mtime > stThumbnail.st_mtime )
       || ( getProcessDate(filename) > stThumbnail.st_mtime ) )
    return false;
  return true;
}

bool prepareThumbnail(const String& root, const String& tmp, const String& filename) {

  if ( isVideo(root + filename) )
    throw VideoException((root+filename).c_str());

  bool isUpToDate=isThumbnailUpToDate(root,tmp,filename);
  if ( ! isUpToDate ) {
    prepareTmpDir(root,tmp,filename);
    makeThumbnail(root,tmp,filename);
  }
  return isUpToDate;
}

