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
#include "common.H"
#include <curl/curl.h>
//#include <curl/types.h>
#include <curl/easy.h>
#include <sys/types.h>
#include <utime.h>

using namespace raii;


String path_encode(const String& str)
{
  if ( str.empty() )
    return String();
  apr_pool_t *pool=apacheRequest->pool;
  char *s=apr_pstrdup(pool,str.c_str());
  int i=0,j=0;
  char *ns=(char*)apr_palloc(pool,strlen(s)*3+1);
  while ( s[i] != '\0' )
  {
    /* if ( s[i] == ' ' )
    {
      ns[j++]='+';
      i++;
    }
    else */ if ( ! ( isalnum(s[i]) || s[i] == '-' || s[i] == '.' || s[i] == '_' || s[i] == '/'  ) )
      j+=sprintf(&ns[j],"%%%02X",(unsigned char)s[i++] );
    else
      ns[j++]=s[i++];

  }
  ns[j]='\0';
  return ns;
}

const char *Dir::months[] = {"janvier","février","mars","avril","mai","juin",
                           "juillet","août","septembre","octobre","novembre","décembre"};
bool sortDir(const Dir& a, const Dir& b)
{
  return a.mtime > b.mtime;
}

String getCurrentUser(HttpServletRequest& request) {
        String user=request.getSession()->getUser();
        if ( user.empty() ) {
                user=request.getRemoteUser();
                if ( user.empty() )
                        user="Anonyme";
        }
        return user;
}

bool isAdmin(const String& user)
{
  Connection conn;
  String req=String("SELECT COUNT(*) AS count FROM gallery_users WHERE name='") + user + "' AND isadmin='true'";
  ResultSet rs=conn.query(req);
  if ( ! rs.next() || rs["count"] == String("0") )
    return false;
  return true;
}

void hitImage(const String& what, const String& path)
{
  String req1=String("SELECT * FROM gallery_stats WHERE path='")+path+String("'");
  Connection conn;
  ResultSet rs=conn.query(req1);
  if ( rs.rowCount() == 0 )
    {
      String req2=String("INSERT INTO gallery_stats (path,preview,download)"
                 " VALUES ('")+path+String("',0,0)");
      conn.query(req2);
    }
  String req3=String("UPDATE gallery_stats SET ")+what+String("=")+what+String("+1 WHERE path='")+path+String("'");
  conn.query(req3);
}


bool sendToPhotoService(const String& filename, const String& user, const String& password) {
        CURL *c;
        char *info;
        char url[2048];
        char *querystring;
        FILE *f=fopen("/dev/null","w");
        static int alreadyInitialized=0;

        if ( ! alreadyInitialized ) {
                curl_global_init(CURL_GLOBAL_ALL);
                alreadyInitialized=1;
        }

        //init
        c=curl_easy_init();
        curl_easy_setopt(c,CURLOPT_WRITEDATA,f);
        curl_easy_setopt(c,CURLOPT_WRITEHEADER ,f);
        curl_easy_setopt(c,CURLOPT_USERAGENT,"NotSoOriginal");
        curl_easy_setopt(c,CURLOPT_COOKIEFILE,"/fichierquinexistepas!!");
        curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1);

        //récupération des cookies et de la redirection vers la page de login...
        curl_easy_setopt(c,CURLOPT_URL,"http://www.photoservice.com/session/loggin.cfm");
        curl_easy_perform(c);
        if ( curl_easy_getinfo(c,CURLINFO_EFFECTIVE_URL,&info) != CURLE_OK ) {
                raii::Logger log("Gallery");
                log(HERE).warning()("ne peut récupérer l'url");
                fclose(f);
                return false;
        }

        //login
        char *vardata=strdupa(info);
        struct curl_httppost *formpost=NULL;
        struct curl_httppost *lastptr=NULL;
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "currentPage",
                     CURLFORM_COPYCONTENTS, vardata,
                     CURLFORM_END);
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "h_login_email",
                     CURLFORM_COPYCONTENTS, user.c_str(),
                     CURLFORM_END);
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "login_email",
                     CURLFORM_COPYCONTENTS, user.c_str(),
                     CURLFORM_END);
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "h_login_pass",
                     CURLFORM_COPYCONTENTS, password.c_str(),
                     CURLFORM_END);
	char *pos=strchr(info,'?');
	if ( !pos ) {
                raii::Logger log("Gallery");
                log(HERE).warning()("querystring vide");
        //        fclose(f);
        //        return false;
		querystring=strdupa("");
	}
	else
		querystring=strdupa(pos);
        snprintf(url,2048,"http://www.photoservice.com/session/loggin.cfm%s",querystring);

        curl_easy_setopt(c,CURLOPT_URL,url);
        curl_easy_setopt(c, CURLOPT_HTTPPOST, formpost);
        curl_easy_perform(c);

        //envoie de l'image

                snprintf(url,2048,"http://www.photoservice.com/tirages/process_transfert_classique.cfm"
                          "%s&ZoneName=tirages&albumaction=&bpage=&addto=&cdyedit=0",querystring);

        struct curl_httppost *formpost2=NULL;
        struct curl_httppost *lastptr2=NULL;
        curl_formadd(&formpost2,
                     &lastptr2,
                     CURLFORM_COPYNAME, "upld1",
                     CURLFORM_FILE, filename.c_str(),
                     CURLFORM_END);


        curl_easy_setopt(c,CURLOPT_URL,url);
        curl_easy_setopt(c, CURLOPT_HTTPPOST, formpost2);
        curl_easy_perform(c);



        curl_easy_cleanup(c);
        fclose(f);
        return true;
}


bool isPathInfoPublic(HttpServletRequest& request, HttpServletResponse& response) {


        String dir=url_decode(request.getPathInfo());

        while (true) {
                int last_slash=dir.rfind("/",dir.size());
                dir=dir.substr(0,last_slash);
                ImageDesc ddesc(path_encode(dir)+"/");

                if ( !ddesc.isPublic ) {
                        return false;
                }
                if ( dir.empty() )
                        break;
        }
        return true;
}

bool isPathInfoHidden(const String& user, HttpServletRequest& request, HttpServletResponse& response) {


        String dir=url_decode(request.getPathInfo());

        while (true) {
                int last_slash=dir.rfind("/",dir.size());
                dir=dir.substr(0,last_slash);
                ImageDesc ddesc(path_encode(dir)+"/");

                if ( !isAdmin(user) && ddesc.isVDir && ddesc.owner != user && !isAdmin(ddesc.owner) )
                        return true;
                if ( ddesc.isHidden ) {
                        return true;
                }
                if ( dir.empty() )
                        break;
        }
        return false;
}


void securityCheck(HttpServlet* servlet, HttpServletRequest& request, HttpServletResponse& response) {

        String user=getCurrentUser(request);
        raii::Logger log("Gallery");


	String path_info = path_encode(url_decode(request.getPathInfo()));

	if ( ! request.getParameter("pass").empty() ) {
		Connection conn;
		ResultSet rs = conn.query("SELECT * FROM gallery_pass where id='"+conn.sqlize(request.getParameter("pass"))+"'");
		if ( rs.rowCount() != 0 ) {
			rs.next();
			request.getSession()->setAttribute("limit",new String(rs["path"]));
			user=rs["name"];
			request.getSession()->setUser(user);
			log.warning(String("User [")+user+"@"+request.getRemoteAddr()+"] logged in with laisser-passer");
			response.sendRedirect(request.getContextPath()+"/gallery.C"+rs["path"]);
		}
		else
			response.sendRedirect(request.getContextPath()+"/gallery.C"+path_info);

	}

	ptr<String> limit = request.getSession()->getAttribute("limit");
	if ( limit && ! limit->empty() && *limit != path_info.substr(0,limit->length()) )
		response.sendRedirect(request.getContextPath()+"/gallery.C"+*limit);

        if ( request.getParameter("action") == String("login") ) {
                user=request.getParameter("login");
                String password=request.getParameter("password");
                Connection conn;
                String req=String("SELECT * FROM gallery_users WHERE name='") + conn.sqlize(user) + "'";
                ResultSet rs=conn.query(req);

                try {
                        request.getSession()->setAttribute("flash",NULL);
                        if ( ! rs.next() )
                                throw 0;
                        String db_password=rs["password"];
                        String us_password=crypt(password.c_str(),db_password.substr(0,2).c_str());

                        if ( db_password != us_password )
                                throw 1;

                        request.getSession()->setUser(user);
                        log.warning(String("User [")+user+"@"+request.getRemoteAddr()+"] logged in");

                        response.sendRedirect(request.getRequestURL());


                }
                catch ( int& e ) {
                        request.getSession()->setAttribute("flash",new String("Utilisateur ou mot de passe incorrect"));
                        log.error(String("Invalid user/password: [")+user+"@"+request.getRemoteAddr()+"] ["+password+"]");
                        sleep(3);
                        //response.sendError(403);
                        response.sendRedirect(request.getRequestURL());
                }
        }
        else if ( user == String("Anonyme") ) {
                if ( ! isPathInfoPublic(request,response ) ) {
                        String dir=url_decode(request.getPathInfo());
                        ImageDesc ddesc(path_encode(dir)+"/");
                        request.getSession()->setAttribute("flash",new String((ddesc.alias.empty()?dir:ddesc.alias) + " est un répertoire privé"));
                        RequestDispatcher rd=request.getRequestDispatcher("/login.csp");
                        rd.forward(request,response);
                }
        }
        else if ( !isAdmin(user) && isPathInfoHidden(user,request,response) ) {
                String dir=url_decode(request.getPathInfo());
                ImageDesc ddesc(path_encode(dir)+"/");
                request.getSession()->setAttribute("flash",new String((ddesc.alias.empty()?dir:ddesc.alias) + " est un répertoire caché"));
                //RequestDispatcher rd=request.getRequestDispatcher("/login.csp");
                //rd.forward(request,response);
                response.sendRedirect(request.getContextPath());
        }

}



bool isVDir(const String& pathInfo) {

        if ( pathInfo == "/" || pathInfo[pathInfo.length()-1] != '/' )
                return false;
        String pathpart=pathInfo.substr(0,pathInfo.rfind("/",pathInfo.length()-2))+"/";
        String filepart=pathInfo.substr(pathInfo.rfind("/",pathInfo.length()-2)+1);
        Connection conn;
        String req=String("SELECT * FROM gallery_vdir WHERE path='")+path_encode(pathpart)+"' AND name ='"+path_encode(filepart)+"'";
        if ( conn.query(req).next() )
                return true;
        return false;
}

String getVDirPathPart(const String& pathInfo) {
        if ( pathInfo == "/" || pathInfo[pathInfo.length()-1] != '/' )
                throw IllegalArgumentException("est / ou pas un répertoire");
        return pathInfo.substr(0,pathInfo.rfind("/",pathInfo.length()-2))+"/";
}

String getVDirDirPart(const String& pathInfo) {
        if ( pathInfo == "/" || pathInfo[pathInfo.length()-1] != '/' )
                throw IllegalArgumentException("est / ou pas un répertoire");
        return pathInfo.substr(pathInfo.rfind("/",pathInfo.length()-2)+1);
}

String getVPhotoRealPath(const String& pathInfo) {
        String realpath;
        if ( pathInfo[pathInfo.length()-1] != '/' ) {
                String pathpart=pathInfo.substr(0,pathInfo.rfind("/")+1);
                String namepart=pathInfo.substr(pathInfo.rfind("/")+1);
                Connection conn;
                String req=String("SELECT * FROM gallery_vphoto WHERE dir='")+path_encode(pathpart)+"' AND name='"+path_encode(namepart)+"'";
                ResultSet rs=conn.query(req);
                if ( rs.next() )
                        realpath=url_decode(rs["realpath"]);
                else
                        realpath=pathInfo;
        }
        else {
                return pathInfo;
        }
        return realpath;
}

bool isVDirOwnedBy(const String& pathInfo,const String& user) {

        if ( pathInfo == "/" || pathInfo[pathInfo.length()-1] != '/' )
                return false;
        String pathpart=pathInfo.substr(0,pathInfo.rfind("/",pathInfo.length()-2))+"/";
        String filepart=pathInfo.substr(pathInfo.rfind("/",pathInfo.length()-2)+1);
        Connection conn;
        String req=String("SELECT * FROM gallery_vdir WHERE path='")+path_encode(pathpart)+"' AND name ='"+path_encode(filepart)+"'";
        ResultSet rs=conn.query(req);
        if ( rs.next() ) {
                if ( isAdmin(user) || rs["owner"] == user )
                        return true;
        }
        return false;
}

Map<String,String> getVDirsForUser(const String& user) {
        Map<String,String> vdirs;
        String userconst="";
        if ( !isAdmin(user) )
                userconst=String(" WHERE owner='")+user+"'";
        Connection conn;
        ResultSet rs=conn.query(String("SELECT * FROM gallery_vdir")+userconst+" ORDER BY date DESC");
        while ( rs.next() ) {
                ImageDesc imgdsc(rs["path"]+rs["name"]);
                vdirs[rs["id"]]= ( imgdsc.alias.empty()?rs["name"].substr(0,rs["name"].length()-1):imgdsc.alias ) + " (dans " + url_decode(rs["path"]) + ")";
        }
        return vdirs;
}
void updateDirDate(String root, String pathInfo, int year, int month, int day) {

        struct tm t;
        time_t timestamp;

        t.tm_sec=0;
        t.tm_min=0;
        t.tm_hour=12;
        t.tm_mday=day;
        t.tm_mon=month-1;
        t.tm_year=year-1900;

        timestamp=mktime(&t);

        if ( isVDir(pathInfo) ) {
                Connection conn;
                String pathPart=getVDirPathPart(pathInfo);
                String dirPart=getVDirDirPart(pathInfo);

                String req=String("UPDATE gallery_vdir SET date='")+ itostring(timestamp)+"' WHERE path='"+path_encode(pathPart)+"' AND name='"+path_encode(dirPart)+"'";
                conn.query(req);
        }
        else {
                struct utimbuf ut;
                ut.actime=timestamp;
                ut.modtime=timestamp;
                if ( utime( (root+pathInfo).c_str() , &ut) )
                        throw IOException("utime");
        }
}

