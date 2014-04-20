
#include <Gallery/common.H>
#include <Magick++.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>

using namespace Magick;

using namespace raii;

class SERVLET(image) : public HttpServlet {

        public:

        void serveThumbnail(const String& t, HttpServletResponse& response,const String& filename) {

                String thumbnail=getInitParameter("TMP") + String("/") + t + getInitParameter("ROOT") + filename + String(".jpg");
                
                response.setContentType("image/jpg");
                FILE *f=fopen(thumbnail.c_str(),"r");
		if ( !f ) throw IOException((thumbnail+": "+strerror(errno)).c_str());
                try {
                        response << f;
                        if (f) fclose(f);
                }
                catch ( ... ) {
                        if (f) fclose(f);
                        response.setContentType("text/html");
                        throw;
                }
        }


        void doThumbnail(const String& realpath, const String& type, HttpServletRequest& request, HttpServletResponse& response) {

                bool isUpToDate;
                String filename = url_decode(realpath);

                struct stat st;
                if ( stat((getInitParameter("ROOT")+filename).c_str(),&st) == -1 )
                        response.sendError(404);

                isUpToDate=prepareThumbnail(getInitParameter("ROOT"),getInitParameter("TMP"),filename);

                long ifModifiedSince=request.getDateHeader("If-Modified-Since");
                if ( isUpToDate && ifModifiedSince == st.st_mtime )
                        response.setStatus(304);
                else {
                        response.setDateHeader("Last-Modified",st.st_mtime);
                        serveThumbnail(type,response,filename);
                        if ( ! isAdmin(getCurrentUser(request)) && type != "thumbnail" )
                                hitImage("preview",path_encode(url_decode(realpath)));
                }
        }


        void doImage(const String& realpath, HttpServletRequest& request, HttpServletResponse& response) {

                String root = getInitParameter("ROOT");
                String filename =  url_decode(realpath);

                struct stat st;
                if ( stat((root+filename).c_str(),&st) == -1 )
                        response.sendError(404);

                long ifModifiedSince=request.getDateHeader("If-Modified-Since");
                if ( ifModifiedSince == st.st_mtime )
                        response.setStatus(304);
                else {
                        if ( getCurrentUser(request) == "Anonyme" ) {
                                request.getSession()->setAttribute("flash",new String("Vous devez être identifé pour pouvoir télécharger les versions HD des photos"));
                                RequestDispatcher rd=request.getRequestDispatcher("/login.csp");
                                rd.forward(request,response);
                        }

                        response.setDateHeader("Last-Modified",st.st_mtime);

                        request_rec *rr=ap_sub_req_lookup_file((root+filename).c_str(),apacheRequest,NULL);
                        if ( ! rr->content_type )
                                throw ServletException("aucun type mime défini pour le fichier demandé");
			String contentType(rr->content_type);
                        ap_destroy_sub_req(rr);


			try {
				//tentative sur processed
				serveThumbnail("processed", response, filename);
				return;
			} catch (...) {}

                        if ( ! isAdmin(getCurrentUser(request)) )
                                hitImage("download",path_encode(url_decode(realpath)));
			
                        response.setContentType(contentType);

                        FILE *f=fopen((root+filename).c_str(),"r");
                        try {
                                response << f;
                                if (f) fclose(f);
                        }
                        catch ( ... ) {
                                if (f) fclose(f);
                                response.setContentType("text/html");
                                throw;
                        }
                }
        }


        void doRotate(const String& realpath, HttpServletRequest& request, HttpServletResponse& response) {

                String filename = getInitParameter("ROOT") + url_decode(realpath);

                double angle;
                Image image;
                try {
                        image.read( filename );
                }
                catch (...) {
                        throw IOException(filename.c_str());
                }

                if ( request.getParameter("direction") == "cw" )
                        angle=90.0;
                else if ( request.getParameter("direction") == "ccw" )
                        angle=-90.0;
                else
                        throw IllegalArgumentException("bad direction");


                image.rotate(angle);

                try {
                        if ( rename(filename.c_str(),(filename+".old").c_str()) < 0 )
                                throw IOException("rename");
                        image.write( filename );
                        if ( unlink((filename+".old").c_str()) < 0 )
                                throw IOException("unkink old file");
                }
                catch ( IOException& e ) {
                        throw;
                }
                catch (...) {
                        throw IOException(filename.c_str());
                }
                response.sendRedirect(response.encodeRedirectURL(
                request.getContextPath() + "/gallery.C" + request.getPathInfo() + "?action=show" )
                );
        }


        void doFlip(const String& realpath, HttpServletRequest& request, HttpServletResponse& response) {

        }


        void doFlop(const String& realpath, HttpServletRequest& request, HttpServletResponse& response) {

        }


        void doExif(const String& realpath, HttpServletRequest& request, HttpServletResponse& response) {

		String pathToImage = getInitParameter("ROOT") + url_decode(realpath);
		ptr<ExifTags> etags=NULL;
                try {
                        ExifTags et(pathToImage);
			etags=new ExifTags(et);
                } catch ( ... ) {
                        etags=new ExifTags();
                }
		op::ImageInfo imageInfo(1,1);
		imageInfo.probeFile(pathToImage);

		if ( ! imageInfo.camera.empty() ) {
			etags->Photo_ISOSpeedRatings = imageInfo.iso_speed;
			if ( etags->Photo_ExposureTime == 0 ) {
				etags->Image_DateTime=imageInfo.timestamp;
				etags->Image_Model=imageInfo.camera;
				etags->Photo_ExposureTime=imageInfo.shutter_speed;
				etags->Photo_FNumber=imageInfo.aperture;
				etags->Photo_FocalLength=ftostring(imageInfo.focal);
			}
			etags->computeAPEX();
		}

                request.setAttribute("exif",etags);
                request.getRequestDispatcher("/exif.csp").include(request,response);


        }


        void service ( HttpServletRequest& request, HttpServletResponse& response) {

                //méchant test, semble que sqlite ne soit pas réentrant { Connection conn ; conn.query("update plop set a=a+1"); }
                securityCheck(this,request,response);
                apacheRequest->no_cache=0;
                apacheRequest->no_local_copy=0;

                String realpath = getRealPath(request.getPathInfo());
                String action=request.getParameter("action");

                if ( action.empty() ) action = "preview";

                if ( action == "thumbnail" || action == "preview" ) {
			try {
                        	doThumbnail(realpath,action,request,response);
			} catch ( VideoException &e ) {
				response.sendRedirect(request.getContextPath()+"/inc/cineThumbnail.jpg");
			}/*
                        catch ( ... ) {
                                response.sendRedirect(request.getContextPath()+"/inc/brokenThumbnail.png");
                        }*/
                }
                else if ( action == "image" ) {
                        doImage(realpath,request,response);
                }
                else if ( action == "rotate" ) {
                        if ( ! isAdmin(getCurrentUser(request)) )
                                throw ServletException("Nécessite les droits administrateur");
                        doRotate(realpath,request,response);
                }
                else if ( action == "flip" ) {
                        if ( ! isAdmin(getCurrentUser(request)) )
                                throw ServletException("Nécessite les droits administrateur");
                        doFlip(realpath,request,response);
                }
                else if ( action == "flop" ) {
                        if ( ! isAdmin(getCurrentUser(request)) )
                                throw ServletException("Nécessite les droits administrateur");
                        doFlop(realpath,request,response);
                }
                else if ( action == "show" || action == "photo-service" ) {
                        // we're included from show.csp
                        doExif(realpath,request,response);
                }
                else
                        throw ServletException("Unknown action");
        }

};
exportServlet(image);
