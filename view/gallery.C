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
#include "raii.H"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>
#include <Gallery/common.H>
#include <algorithm>

using namespace raii;


class SERVLET(gallery) : public HttpServlet {

        ptr<IndexAttribute> createIndexAttribute (  HttpServletRequest& request, HttpServletResponse& response) {

                String pathInfoEncoded=request.getPathInfo();

                pathInfoEncoded=pathInfoEncoded.substr(0,pathInfoEncoded.rfind("/"))+"/";

                String pathInfo=url_decode(pathInfoEncoded);

                ptr<IndexAttribute> cnt = new IndexAttribute();
                Vector<Dir> dirs;


                // real dir

                struct dirent **entry;
                int m = scandir((getInitParameter("ROOT")+pathInfo).c_str(),&entry,0,versionsort);
                int n;
                for (n = 0 ; n < m ; n++ ) {

                        String filename=pathInfo /* + String("/") */
                        + String(entry[n]->d_name);

                        bool forcedir=false;
                        if ( entry[n]->d_type & DT_LNK ) {
                                struct stat st;
                                if ( stat(String(getInitParameter("ROOT")+filename+String("/")).c_str(),&st) == 0  ) {
                                        if ( S_ISDIR(st.st_mode) )
                                                forcedir=true;
                                }
                        }

			if ( entry[n]->d_name[0] == '.' ) {
				// noop, n'affiche pas les fichiers cachés
			} else if ( forcedir || entry[n]->d_type & DT_DIR ) {
                                dirs.push_back(Dir(getInitParameter("ROOT"),pathInfo,entry[n]->d_name));
                        }
                        else { /* il s'agit d'un fichier */
                                request_rec *rr=ap_sub_req_lookup_file(
                                                 (getInitParameter("ROOT")+pathInfo+String(entry[n]->d_name)).c_str(),
                                                 apacheRequest,
                                                 NULL);
                                if ( rr ) {
                                        if (rr->content_type
                                            && (( rr->content_type[0] == 'i'
                                            && rr->content_type[1] == 'm'
                                            && rr->content_type[2] == 'a'
                                            && rr->content_type[3] == 'g'
                                            && rr->content_type[4] == 'e' )||(
					     rr->content_type[0] == 'v'
					    && rr->content_type[1] == 'i'
					    && rr->content_type[2] == 'd'
					    && rr->content_type[3] == 'e'
					    && rr->content_type[4] == 'o'

					    ) ) ) {
                                                 ImageDesc imgdsc(path_encode(pathInfo)
                                                                  +path_encode(String(entry[n]->d_name)));
                                                 cnt->files << "<a title=\""
                                                            << ( imgdsc.alias.empty()?String(entry[n]->d_name):imgdsc.alias )
                                                            << "\" href=\""
                                                            << request.getContextPath()
                                                            << "/gallery.C"
                                                            << path_encode(filename)
                                                            << "?action=show\"><img alt=\""
                                                            << String(entry[n]->d_name)
                                                            << "\"src=\""
                                                            << request.getContextPath()
                                                            << "/image.C"
                                                            << path_encode(filename)
                                                            << "?action=thumbnail\"/></a>";
                                                cnt->links.push_back(
                                                     Link(( imgdsc.alias.empty()?String(entry[n]->d_name):imgdsc.alias ),
                                                         path_encode(filename),
                                                         String(entry[n]->d_name)));

                                        }
                                ap_destroy_sub_req(rr);
                                }
                                else {
                                        cnt->files << "<a>"<< filename <<" : Erreur</a>";
                                }
                        }
                        free(entry[n]);
                }

                bool rdir=false;

                if ( m > 0 ) {
                        free(entry);
                        rdir=true;
                }


                //  vdir
                bool vdir=false;

                Connection conn;

                String path=pathInfo.substr(0,pathInfo.length()-1);
                String vpath=path.substr(0,path.rfind("/"));
                /*if ( vpath.empty() )
                        vpath="/";
                        */
                        vpath+="/";
                String vname=path.substr(path.rfind("/")+1);
                if ( ! vname.empty() )
                        vname+="/";

                String req=String("SELECT * FROM gallery_vdir WHERE path='")+path_encode(vpath)+"'"
                              + " AND name='"+path_encode(vname)+"'";
                if ( conn.query(req).next() )
                        vdir=true;

                //if ( pathInfo == "/"  ) {

                        req=String("SELECT * FROM gallery_vdir WHERE path='")+path_encode(vpath+vname)+"'";
                        ResultSet rs=conn.query(req);
                        while ( rs.next() ) {
                                //if ( getCurrentUser(request) == rs["owner"] || isAdmin(getCurrentUser(request)) )
                                String path=url_decode(rs["name"]);
                                path=path.substr(0,path.length()-1);
                                dirs.push_back(Dir(getInitParameter("ROOT"),pathInfo,path,atoi(rs["date"].c_str())));
                                vdir=true;
                        }

                        rs=conn.query(String("SELECT * FROM gallery_vphoto WHERE dir='")+path_encode(pathInfo)+"' ORDER BY name");
                        while ( rs.next() ) {
                                String filename=pathInfo + rs["name"];
                                ImageDesc imgdsc(path_encode(pathInfo)
                                          +path_encode(String(rs["name"])));
                                cnt->files << "<a title=\""
                                        << ( imgdsc.alias.empty()?rs["name"]:imgdsc.alias )
                                        << "\" href=\""
                                        << request.getContextPath()
                                        << "/gallery.C"
                                        << path_encode(filename)
                                        << "?action=show\"><img alt=\""
                                        << String(rs["name"])
                                        << "\"src=\""
                                        << request.getContextPath()
                                        << "/image.C"
                                        << path_encode(filename)
                                        << "?action=thumbnail\"/></a>";
                                cnt->links.push_back(Link(( imgdsc.alias.empty()?rs["name"]:imgdsc.alias ),
                                                         path_encode(filename),
                                                         String(rs["name"])));

                                vdir=true;
                        }
                //}


                if ( !rdir && !vdir ) {
                        response.sendError(404);
                }

                // classement

                std::sort(dirs.begin(),dirs.end(),sortDir);

                int year=0;
                int month=0;
                String user=getCurrentUser(request);
                bool showSecuredWhenNotLogged( getInitParameter("ShowSecured") == "true" );
                bool isAnonyme=( user == "Anonyme" );
                bool showLocks=( getInitParameter("ShowLocks") == "true" );
                bool adminShowPerms=( getInitParameter("AdminShowPerms") == "true" );
                bool adminMode=isAdmin(user);

                for ( Vector<Dir>::const_iterator it = dirs.begin() ;
                it != dirs.end() ;
                it ++ ) {
                        if ( isAnonyme &&  !it->isPublic && !showSecuredWhenNotLogged )
                                continue;
                        if ( !adminMode ) {
                                if ( it->owner != user ) {
                                        if ( it->isHidden ) {
                                                        continue;
                                        }
                                        if ( it->isVDir ) {
                                                if ( !isAdmin(it->owner) ) {
                                                        continue;
                                                }
                                        }
                                }
                        }

                        if ( it->year != year ) {
                                if ( year != 0 )
                                        cnt->dirs << "</div></div>";
                                year=it->year;
                                month=it->month;
                                cnt->dirs << "<div class=\"year\"><h3>"
                                          << it->year << "</h3>"
                                          << "<div class=\"month\"><h4>"
                                          << it->hrmonth << "</h4>";
                        }
                        if ( it->month != month ) {
                                month=it->month;
                                cnt->dirs << "</div><div class=\"month\"><h4>"
                                          << it->hrmonth << "</h4>";
                        }

                        String linkclass=( (it->isPublic || ( !showLocks ) || !isAnonyme)?"":" class=\"restricted\"" );
                        if ( adminMode && adminShowPerms ) {
                                if ( ! it->isPublic )
                                        linkclass=" class=\"restricted\"";
                                if (  it->isHidden )
                                        linkclass=" class=\"ghost\"";
                        }
                        cnt->dirs << "<p" <<  linkclass  << "><a title =\""
                                  << it->desc
                                  << "\" href=\""
                                  << path_encode(it->href)
                                  << "/\">" << it->a
                                  << "</a>";
                        if ( ! it->desc.empty() ) {
                                cnt->dirs << "<span class=\"desc\">" << it->desc << "</span>";
                        }
                        cnt->dirs << "<span class=\"date\">" << it->day << " " << it->hrmonth << "</span></p>";

                }

                if ( ! dirs.empty() )
                        cnt->dirs << "</div></div>";

                ptr<IndexCache> indexCache=request.getSession()->getAttribute("IndexCache");

                if ( !indexCache) {
                        indexCache=new IndexCache();
                        request.getSession()->setAttribute("IndexCache",indexCache);
                }
                indexCache->setEntry(pathInfo,cnt);

                return cnt;
        }


        void updatedir(HttpServletRequest& request, HttpServletResponse& response) {

                String pathInfo=url_decode(request.getPathInfo());
                pathInfo=pathInfo.substr(0,pathInfo.rfind("/"))+"/";

                String pragma(request.getHeader("Pragma"));

                ptr<IndexAttribute> cnt=NULL;
                ptr<IndexCache> indexCache=NULL;

                if (  pragma.empty() || pragma != String("no-cache") ) {
                        indexCache = request.getSession()->getAttribute("IndexCache");
                        if ( !indexCache) {
                                indexCache=new IndexCache();
                                request.getSession()->setAttribute("IndexCache",indexCache);
                        }
                        cnt = indexCache->getEntry(pathInfo);
                }

                if ( ! cnt )
                        cnt=createIndexAttribute(request,response);
        }


        void doIndex (  HttpServletRequest& request, HttpServletResponse& response) {

                updatedir(request,response);
                RequestDispatcher rd=request.getRequestDispatcher("/index.csp");
                rd.forward(request,response);
        }


        void doShow ( HttpServletRequest& request, HttpServletResponse& response) {

                updatedir(request,response);
                RequestDispatcher rd=request.getRequestDispatcher("/show.csp");
                rd.forward(request,response);
        }



        void doPost( HttpServletRequest& request, HttpServletResponse& response) {

                String commentname=request.getParameter("commentname");
                String commentdata=request.getParameter("commentdata");

                Connection conn;
                ResultSet rs=conn.query(String("INSERT INTO gallery_entry (path,author,description) VALUES"
                             "('") + path_encode(getVPhotoRealPath(url_decode(request.getPathInfo())) ) + String ("','") + url_encode(commentname)
                             + String("','") + url_encode (commentdata) + String ("')"));

                response.sendRedirect(response.encodeRedirectURL(request.getContextPath() + String("/gallery.C") + path_encode(request.getPathInfo()) + String("?action=show") ));
                //RequestDispatcher rd=request.getRequestDispatcher("/show.csp");
                //rd.forward(request,response);
        }


        void saveAdminData( HttpServletRequest& request, HttpServletResponse& response) {

                if ( ! isAdmin(getCurrentUser(request)) )
                        throw ServletException("Vous n'êtes pas administrateur");
                String pathInfo= url_decode(request.getPathInfo());
                String commentname=request.getParameter("commentname");
                String commentphoto=request.getParameter("commentphoto");
                String commentdata=request.getParameter("commentdata");
                String commentperms=request.getParameter("commentperms");

                String dirdate_day=request.getParameter("dirdate_day");
                String dirdate_month=request.getParameter("dirdate_month");
                String dirdate_year=request.getParameter("dirdate_year");

                if ( pathInfo != "/" && !dirdate_day.empty() && !dirdate_month.empty() && !dirdate_year.empty() ) {
                        int year=atoi(dirdate_year.c_str()),
                            month=atoi(dirdate_month.c_str()),
                            day=atoi(dirdate_day.c_str());
                            try {
                                    updateDirDate(getInitParameter("ROOT"),pathInfo,year,month,day);
                            }
                            catch ( IOException& e ) {
                                    request.getSession()->setAttribute("flash",new String("Changement de date impossible"));
                            }
                }

                Connection conn;
                ResultSet rs=conn.query(String( "SELECT COUNT(*) AS count"
                                        " FROM gallery_entry"
                                        " WHERE created_at is NULL"
                                        " AND path='" ) + path_encode(getVPhotoRealPath(pathInfo)) + String ( "'" ) );
                rs.next();
                if ( rs["count"] == "0" ) {
                        conn.query(String ("INSERT INTO gallery_entry (created_at,path,author,alias,description,secured) VALUES"
                                   " (NULL,'" ) + path_encode(getVPhotoRealPath(pathInfo)) + String( "','" ) + url_encode(commentname)
                                   + String("','") + url_encode(commentphoto) +String("','") + url_encode(commentdata) + "','" + commentperms + "')" );
                }
                else {
                        conn.query(String("UPDATE gallery_entry SET author='") + url_encode(commentname)
                                   + "', alias='" + url_encode(commentphoto) + "', description='" + url_encode(commentdata)
                                   + "',secured='" + commentperms + "' WHERE created_at is NULL AND path='" + path_encode(getVPhotoRealPath(pathInfo)) + "'");
                }

                //purge du cache
                /*
                ptr<HttpSession> session=request.getSession();
                Vector<String> attrs=session->getAttributeNames();
                for ( Vector<String>::const_iterator it=attrs.begin();
                      it != attrs.end();
                      it++ ) {
                        session->setAttribute(*it,NULL);
                }*/
                request.getSession()->setAttribute("IndexCache",new IndexCache());
        }


        void doAdminImage( HttpServletRequest& request, HttpServletResponse& response) {

                saveAdminData(request,response);
                response.sendRedirect(response.encodeRedirectURL(request.getContextPath() + String("/gallery.C")
                                + path_encode(request.getPathInfo()) + String("?action=show") ));
        }


        void doAdminDir( HttpServletRequest& request, HttpServletResponse& response) {

                saveAdminData(request,response);
                //purge du cache: il faudrait aussi purger le cache du répertoire parent
                //getServletContext()->setAttribute(url_decode(request.getPathInfo()),NULL);
                response.sendRedirect(response.encodeRedirectURL( request.getContextPath() + String("/gallery.C")
                                        + path_encode(request.getPathInfo()) ));
        }


        void doRemovePost( HttpServletRequest& request, HttpServletResponse& response) {

                if ( ! isAdmin(getCurrentUser(request)) )
                        throw ServletException("Vous n'êtes pas administrateur");
                if ( request.getParameter("id").empty() )
                        throw ServletException("id vide !");
                Connection conn;
                String req=String("DELETE FROM gallery_entry WHERE id = ")
                            + request.getParameter("id")
                            + String(" AND path='") + path_encode(getVPhotoRealPath(url_decode(request.getPathInfo())))+ String("'");
                ResultSet rs=conn.query(req);
                response.sendRedirect(response.encodeRedirectURL(request.getContextPath() + String("/gallery.C") + path_encode(request.getPathInfo()) + String("?action=show") ));
        }


        void doListContextAttributes( HttpServletRequest& request, HttpServletResponse& response) {

                RequestDispatcher rd=request.getRequestDispatcher("/list.csp");
                rd.forward(request,response);
        }


        void doPhotoService( HttpServletRequest& request, HttpServletResponse& response) {

                if ( getCurrentUser(request) == "Anonyme" ) {
                        response.sendError(403);
                }

                if ( !request.getParameter("phsv-login").empty() ) {
                        request.getSession()->setAttribute("phsv-login",new String(request.getParameter("phsv-login")));
                        request.getSession()->setAttribute("phsv-password",new String(request.getParameter("phsv-password")));
                }
                ptr<String> login=request.getSession()->getAttribute("phsv-login");
                ptr<String> password=request.getSession()->getAttribute("phsv-password");

                if ( !login || login->empty() ) {
                        request.setAttribute("phsv-login",new String("fait le"));
                        updatedir(request,response);
                        RequestDispatcher rd=request.getRequestDispatcher("/show.csp");
                        rd.forward(request,response);
                }

                String filename = getInitParameter("ROOT") + getVPhotoRealPath(url_decode(request.getPathInfo()));
                bool ret=sendToPhotoService(filename,*login,*password);
                response.sendRedirect(response.encodeRedirectURL(request.getContextPath() + String("/gallery.C") + path_encode(request.getPathInfo()) + String("?action=show&photo-service=") + String( ret?"OK!":"PAS OK!" ) ));
        }

        void doLogin(HttpServletRequest& request, HttpServletResponse& response) {
                if ( getCurrentUser(request) == "Anonyme" ) {
                        RequestDispatcher rd=request.getRequestDispatcher("/login.csp");
                        rd.forward(request,response);
                }
                else
                        doIndex(request,response);
        }

        void doAdminUsers(HttpServletRequest& request, HttpServletResponse& response) {

                if ( ! isAdmin(getCurrentUser(request)) )
                        throw ServletException("Vous n'êtes pas administrateur");

                if ( ! request.getParameter("newuser").empty() ) {
                        //nouvel utilisateur
                        if ( request.getParameter("newuser").size() < 2 ) {
                                request.getSession()->setAttribute("flash",
                                        new String("Le nom d'utilisateur doit faire plus de 2 lettres"));
                        }
                        else if ( request.getParameter("password").empty() ) {
                                request.getSession()->setAttribute("flash",
                                        new String("Le mot de passe ne doit pas être vide"));
                        }
                        else {
                                Connection conn;
                                ResultSet rs=conn.query(
                                String("INSERT INTO gallery_users ( name,password,isadmin) VALUES (")
                                + "'" +request.getParameter("newuser")+ "','" +
                                crypt(request.getParameter("password").c_str(),request.getParameter("newuser").substr(0,2).c_str())
                                + "','" + ( request.getParameter("isadmin").empty()?"false":"true") + "')");
                        }
                }
                else if ( ! request.getParameter("delete").empty() ) {
                        //virer
                        Connection conn;
                        String req=String("DELETE FROM gallery_users WHERE id='")+conn.sqlize(request.getParameter("delete"))+"'";
                        conn.query(req);

                }

                RequestDispatcher rd=request.getRequestDispatcher("/users.csp");
                rd.forward(request,response);
        }


        bool createVDir(const String& user, const String& base, const String& name) {

                if ( ! isVDir(base+name+"/") )  {

                        char now[32];
                        snprintf(now,32,"%d",(int)time(NULL));
                        Connection conn;
                        String req=String("INSERT INTO gallery_vdir (owner,path,name,date) VALUES"
                             "('") + user
                             + "','" + path_encode(base) + "','" + path_encode(name) + "/"
                             + "'," + now + ")";
                        conn.query(req);
                        return true;
                }
                return false;
        }


        void doManageVDir(HttpServletRequest& request, HttpServletResponse& response) {

                String vdir_name=request.getParameter("vdir_name");

                if ( vdir_name.find("/") == String::npos && !vdir_name.empty() ) {

                        if ( isAdmin(getCurrentUser(request)) ) {

                                if ( !createVDir(getCurrentUser(request), url_decode(request.getPathInfo()), vdir_name) )
                                        request.getSession()->setAttribute("flash",new String("cet album existe déjà"));
                        }
                        else if ( getInitParameter("AllowUserVDir") == "true" ) {

                                //si path n'appartient pas à user alors créer le home puis créer le répertoire dans le home
                                if ( isVDirOwnedBy(url_decode(request.getPathInfo()),getCurrentUser(request)) ) {

                                        if ( !createVDir(getCurrentUser(request),url_decode(request.getPathInfo()),vdir_name) )
                                                request.getSession()->setAttribute("flash",new String("cet album existe déjà"));

                                }
                                else {
                                        if ( ! isVDir(String("/~")+getCurrentUser(request)+"/") ) {

                                                createVDir(getCurrentUser(request),"/",String("~")+getCurrentUser(request));
                                                Connection conn;
                                                if ( !conn.query(String("SELECT * FROM gallery_entry WHERE path='")+path_encode(String("/~")+url_decode(getCurrentUser(request)))+"/'").next() ) {
                                                       conn.query(String("INSERT INTO gallery_entry (created_at,author,path,alias,secured) VALUES ")
                                                                + "(NULL,'"+path_encode(getCurrentUser(request))+"','"+path_encode(String("/~")
                                                                +url_decode(getCurrentUser(request))+"/")+"','albums de "+path_encode(getCurrentUser(request))+"','true')");
                                               }
                                        }



                                        if ( !createVDir(getCurrentUser(request),String("/~")+getCurrentUser(request)+"/",vdir_name) ) {
                                                request.getSession()->setAttribute("flash",new String("cet album existe déjà"));
                                        }


                                }

                        }
                        else
                                request.getSession()->setAttribute("flash",new String("Vous n'êtes pas authorisé à faire ça"));
                }
                else {
                        request.getSession()->setAttribute("flash",new String("nom d'album invalide"));
                }

                request.getSession()->setAttribute("IndexCache",new IndexCache());

                response.sendRedirect(response.encodeRedirectURL(request.getContextPath() + String("/gallery.C") + path_encode(request.getPathInfo() )));
                //RequestDispatcher rd=request.getRequestDispatcher("/show.csp");
                //rd.forward(request,response);

        }

        void doRemoveVDir(HttpServletRequest& request, HttpServletResponse& response) {

                String pathInfo=url_decode(request.getPathInfo());
                String pathpart=pathInfo.substr(0,pathInfo.rfind("/",pathInfo.length()-2))+"/";
                String filepart=pathInfo.substr(pathInfo.rfind("/",pathInfo.length()-2)+1);

                if (  isAdmin(getCurrentUser(request))
                || isVDirOwnedBy(pathInfo,getCurrentUser(request)) ) {

                        char len[32];
                        snprintf(len,32,"%d",(int)path_encode(pathInfo).length());
                        Connection conn;
                        String req= String("SELECT * FROM gallery_vdir WHERE substr(path,1,")
                                   + len + ") = '"+path_encode(pathInfo)+"'";
                        if (  conn.query(req).next() ) {
                                request.getSession()->setAttribute("flash",new String("cet album n'est pas vide"));
                        } else {
                                conn.query(String("DELETE FROM gallery_vphoto WHERE dir = '")+path_encode(pathInfo)+"'");
                                conn.query(String("DELETE FROM gallery_vdir WHERE path='")+path_encode(pathpart)+"' AND name='"+path_encode(filepart)+"'");

                        request.getSession()->setAttribute("flash",new String("vdir supprimé"));
                        request.getSession()->setAttribute("IndexCache",new IndexCache());
                        response.sendRedirect(response.encodeRedirectURL(request.getContextPath()
                            + String("/gallery.C") + path_encode( pathpart )));
                        }
                }
                else {
                        request.getSession()->setAttribute("flash",new String("vous n'avez pas le droit de faire ça"));
                }
                response.sendRedirect(response.encodeRedirectURL(request.getContextPath()
                            + String("/gallery.C") + path_encode( pathInfo )));

        }


        void doRemoveFromVDir(HttpServletRequest& request, HttpServletResponse& response) {

                String pathInfo=url_decode(request.getPathInfo());
                String pathpart=pathInfo.substr(0,pathInfo.rfind("/",pathInfo.length()-2))+"/";
                String filepart=pathInfo.substr(pathInfo.rfind("/",pathInfo.length()-2)+1);
                String photo_id=request.getParameter("photo_id");
                if (  isAdmin(getCurrentUser(request))
                || isVDirOwnedBy(pathInfo,getCurrentUser(request)) ) {

                        Connection conn;
                        conn.query(String("DELETE FROM gallery_vphoto WHERE dir = '")+path_encode(pathInfo)+"' AND name='"+path_encode(photo_id)+"'");

                        request.getSession()->setAttribute("flash",new String("photo retirée"));
                        request.getSession()->setAttribute("IndexCache",new IndexCache());
                }
                else {
                        request.getSession()->setAttribute("flash",new String("vous n'avez pas le droit de faire ça"));
                }
                response.sendRedirect(response.encodeRedirectURL(request.getContextPath()
                            + String("/gallery.C") + path_encode( pathInfo )));


        }

        void doCopyToVDir(HttpServletRequest& request, HttpServletResponse& response) {

                String album_id=request.getParameter("album_id");
                if ( ! album_id.empty() ) {
                        Connection conn;
                        ResultSet rs=conn.query(String("SELECT * FROM gallery_vdir WHERE id='")+url_encode(album_id)+"'");
                        if ( rs.next() ) {
                                String user=getCurrentUser(request);
                                bool admin=isAdmin(user);
                                if ( admin || user == rs["owner"] ) {
                                        //ok
                                        String pathpart=url_decode(request.getPathInfo()).substr(0,request.getPathInfo().rfind("/")) + "/";
                                        String namepart=url_decode(request.getPathInfo()).substr(request.getPathInfo().rfind("/")+1);
                                        String realpath;
                                        String req=String("SELECT * FROM gallery_vphoto WHERE dir='")+path_encode(pathpart)+"' AND name='"+path_encode(namepart)+"'";
                                        ResultSet rs2=conn.query(req);
                                        if ( rs2.next() )
                                                realpath=rs2["realpath"];
                                        else
                                                realpath=request.getPathInfo();

                                        conn.query(String("INSERT INTO gallery_vphoto(dir,realpath)VALUES('")+rs["path"]+rs["name"]+"','"+path_encode(url_decode(realpath))+"')");
                                        request.getSession()->setAttribute("IndexCache",new IndexCache());
                                        request.getSession()->setAttribute("lastvdir",new String(album_id));
                                }
                                else {
                                        request.getSession()->setAttribute("flash",new String("cet album n'est pas à vous"));
                                }
                        }
                        else {
                                request.getSession()->setAttribute("flash",new String("cet album n'existe pas"));
                        }
                }
                else {
                        request.getSession()->setAttribute("flash",new String("id vide"));
                }
        response.sendRedirect(response.encodeRedirectURL(request.getContextPath()
            + String("/gallery.C") + path_encode( url_decode(request.getPathInfo() ))+"?action=show"));

        }

	void doGenLaisserPasser(HttpServletRequest& request, HttpServletResponse& response) {

		const char chars[]="012345678901234567890"
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMN0PQRSTUVWXYZ";
		String lp;
		srandom(time(NULL));
		for ( int i = 0 ; i < 20 ; ++i )
			lp+=chars[random()%(sizeof(chars)-1)];
		String path = path_encode( url_decode(request.getPathInfo() ));

		Connection conn;
		conn.query("INSERT INTO gallery_pass (id,name,path) VALUES ('"+lp+"','"+
			conn.sqlize(request.getParameter("laisserpasser")).replace(" ","_")+"','"+conn.sqlize(path)+"')");
		request.getSession()->setAttribute("flash",new String("Nouveau laisser-passer : http://" + request.getServerName() + request.getContextPath() +"/gallery.C?pass="+lp));
		response.sendRedirect(response.encodeRedirectURL(request.getContextPath()
		+ String("/gallery.C") + path ));
	}

        void service ( HttpServletRequest& request, HttpServletResponse& response) {

                securityCheck(this,request,response);

                if ( request.getPathInfo().empty() ) {
                        response.sendRedirect(response.encodeRedirectURL(request.getContextPath() + "/gallery.C/"));
                        return;
                }

                String action=request.getParameter("action");

                if ( action.empty() ||
                        action == "index" )
                        doIndex(request,response);
                else if ( action == "show" )
                        doShow(request,response);
                else if ( action == "post" )
                        doPost(request,response);
                else if ( action == "admin_image" )
                        doAdminImage(request,response);
                else if ( action == "admin_dir" )
                        doAdminDir(request,response);
                else if ( action == "remove_post" )
                        doRemovePost(request,response);
                else if ( action == "list" )
                        doListContextAttributes(request,response);
                else if ( action == "manage_vdir" )
                        doManageVDir(request,response);
                else if ( action == "remove_vdir" )
                        doRemoveVDir(request,response);
                else if ( action == "remove_from_vdir" )
                        doRemoveFromVDir(request,response);
                else if ( action == "copyto_vdir")
                        doCopyToVDir(request,response);
                else if ( action == "photo-service" )
                        doPhotoService(request,response);
                else if ( action == "admin" )
                        doAdminUsers(request,response);
                else if ( action == "logmein" )
                        doLogin(request,response);
		else if ( action == "genlaisserpasser" )
			doGenLaisserPasser(request,response);
                else
                        throw ServletException("Unknown action");
        }
};

exportServlet(gallery);
