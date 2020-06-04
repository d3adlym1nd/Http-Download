#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<cstring>
#include<csignal>
#include<cstdlib>
#include<unistd.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>
#include<errno.h>

#include<openssl/ssl.h>
#include<openssl/err.h>

extern int errno;

#define Error() std::cout<<"Error ["<<errno<<"] "<<strerror(errno)<<'\n'

class Downloader{
	private:
		int sckSocket;
	public:
		bool isSSL = false;
		
		void SplitString(const char*, char, std::vector<std::string>&, int);
		int StrToint(const char*);
		
		int InitSocket(const char*, const char*);
		bool Download(const char*);
};

int Downloader::StrToint(const char* cBuffer){
	int iLen = strlen(cBuffer);
	int iLen2 = iLen;
	int iRet = 0;
	for(int iIt = 0; iIt < iLen; iIt++){
		int iTlen = 1;
		--iLen2;
		for(int iIt2 = 0; iIt2 < iLen2; iIt2++){
			iTlen *= 10; //decimal
		}
		int iT = cBuffer[iIt] - 48;
		iRet += (iTlen * iT);
	}
	return iRet;
}

void Downloader::SplitString(const char* cBuffer, char cDelimiter, std::vector<std::string>& vcString, int iMax){
	int iStrLen = 0, iIt = 0, iTmp = 0;
	iStrLen = strlen(cBuffer);
	for(; iIt<iStrLen; iIt++){
		std::string strTmp = "";
		while(cBuffer[iIt] != cDelimiter && cBuffer[iIt] != '\0'){
			strTmp.append(1, cBuffer[iIt++]);
		}
		vcString.push_back(strTmp);
		if(++iTmp == iMax){ break; }
	}
}

int Downloader::InitSocket(const char* cHostname, const char* cPort){
	struct addrinfo strctAd, *strctP, *strctServer;
	memset(&strctAd, 0, sizeof(strctAd));
	strctAd.ai_family = AF_UNSPEC;
	strctAd.ai_socktype = SOCK_STREAM;
	int iRet = getaddrinfo(cHostname, cPort, &strctAd, &strctServer);
	if(iRet != 0){
		std::cout<<"getaddrinfo error: "<<gai_strerror(iRet)<<'\n';
		return -1;
	}
	for(strctP = strctServer; strctP != nullptr; strctP = strctP->ai_next){
		if((sckSocket = socket(strctP->ai_family, strctP->ai_socktype, strctP->ai_protocol)) == -1){
			std::cout<<"Socker error\n";
			Error();
			continue;
		}
		if(connect(sckSocket, strctP->ai_addr, strctP->ai_addrlen) == -1){
			std::cout<<"Error connecting\n";
			Error();
			continue;
		}
		break;
	}
	if(strctP == nullptr){
		freeaddrinfo(strctServer);
		return -1;
	}
	freeaddrinfo(strctServer);
	return this->sckSocket;
}

bool Downloader::Download(const char* cUrl){
	std::vector<std::string> vcUrl;
	SplitString(cUrl, '/', vcUrl, 50); //increase this for larger subdirectories or url
	if(vcUrl.size() < 2){
		std::cout<<"Unable to parse url "<<cUrl<<'\n';
		return false;
	}
	
	int iErr = 0, iBytesWrited = 0, iLen = 0, iBytesReaded = 0;
	char cRemotePort[7];
	char cBuffer[2048], cHostname[128], cTmp[128];
	char *cToken = nullptr;
	
	std::string strTmpFileName = vcUrl[vcUrl.size()-1];
	strTmpFileName.append(".t3mp");
	std::ofstream sFile;
	
	SSL *ssl;
	
	if(vcUrl[0] == "https:"){
		isSSL = true;
	}
	
	std::string strHeader = "GET ";
	for(int iIt2 = 3; iIt2<int(vcUrl.size()); iIt2++){
		strHeader.append(1, '/');
		strHeader.append(vcUrl[iIt2]);
	}
	strHeader.append(" HTTP/1.1\r\nHost: ");
	strHeader.append(vcUrl[2]);
	strHeader.append("\r\nConnection: Keep-Alive\r\n\r\n");
	
	strncpy(cTmp, vcUrl[2].c_str(), 127);
	cToken = strtok(cTmp, ":");
	if(cToken != nullptr){
		strncpy(cHostname, cToken, 127);
		cToken = strtok(nullptr, ":");
		if(cToken != nullptr){
			strncpy(cRemotePort, cToken, 5);
		}
	}
	
	if(strlen(cHostname) == 0){
		//Error parsing
		strncpy(cHostname, vcUrl[2].c_str(), 127);
	}
	
	if(strlen(cRemotePort) == 0){
		if(isSSL){
			strncpy(cRemotePort, "443", 4);
		} else {
			strncpy(cRemotePort, "80", 3);
		}
	}
	
	if(isSSL){
		SSL_library_init();
		SSLeay_add_ssl_algorithms();
	}
	
	
	while(1){ //main until receive 200 ok to procede download
		SSL *ssl1;
		if(InitSocket(cHostname, cRemotePort) == -1){
			//function already show error
			//goto release ssl
		}
		if(isSSL){
			ssl1 = SSL_new(SSL_CTX_new(TLS_method()));
			if(ssl1 == nullptr){
				std::cout<<"Error creating ssl object\n";
				ERR_print_errors_fp(stderr);
				//goto release ssl
			}
			SSL_set_fd(ssl1, sckSocket);
			iErr = SSL_connect(ssl1);
			if(iErr == -1){
				SSL_free(ssl1);
				std::cout<<"Error establishing SSL connection\n";
				Error();
				ERR_print_errors_fp(stderr);
				//goto release ssl
			}
			iLen = strHeader.length();
			iBytesWrited = SSL_write(ssl1, (const char *)strHeader.c_str(), iLen);
		} else {
			iBytesWrited = send(sckSocket, strHeader.c_str(), iLen , 0);
		}
		
		if(iBytesWrited > 0){
			memset(cBuffer, 0, 2048);
			if(isSSL){
				iBytesReaded = SSL_read(ssl1, cBuffer, 2048);
			} else {
				iBytesReaded = recv(sckSocket, cBuffer, 2048, 0);
			}
			
			if(iBytesReaded > 0){
				
			} else {
				SSL_free(ssl1);
				std::cout<<"Got no response from server\n";
				Error();
				//goto release ssl
			}
		} else {
			SSL_free(ssl1);
			std::cout<<"Unable to send packet\n";
			Error();
			//goto release ssl
			
		}
		
	}
	
	return true;
	
}


int main(){
	Downloader esta;
	esta.Download("http://www.some-site.com:8980/path/super/long/to/file.executable");
	return 0;
}
