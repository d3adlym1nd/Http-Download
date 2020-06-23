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

#define error() std::cout<<"Error ["<<errno<<"] "<<strerror(errno)<<'\n'

class Downloader{
	private:
		int sckSocket = -1;
	public:
		bool isSSL = false;
		bool bFlag = false;
		
		void SplitString(const char*, char, std::vector<std::string>&, int);
		void ProgressBar(unsigned long int, unsigned long int);
		unsigned long int StrToint(const char*);
		
		int InitSocket(const char*, const char*);
		bool Download(const char*);
};

void Downloader::ProgressBar(unsigned long int value, unsigned long int total){
	int h = 0, hh = 0;
	char pb[101];
	memset(pb, 0, 101);
	int value2 = ((float)value / (float)total) *100;
	for(h=0; h<50; h++){
			for(hh=h; hh<(value2 / 2); hh++, h++){
					pb[hh] = '#';
			}
			pb[h] = '_';
	}
	pb[50] = '\0';
	std::cout<<'\r'<<pb<<'['<<value2<<"%]";
	}

unsigned long int Downloader::StrToint(const char* cBuffer){
	int iLen = strlen(cBuffer);
	int iLen2 = iLen;
	unsigned long int iRet = 0;
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
	strctAd.ai_protocol = IPPROTO_TCP;
	int iRet = getaddrinfo(cHostname, cPort, &strctAd, &strctServer);
	if(iRet != 0){
		std::cout<<"getaddrinfo error: "<<gai_strerror(iRet)<<'\n';
		return -1;
	}
	for(strctP = strctServer; strctP != nullptr; strctP = strctP->ai_next){
		if((sckSocket = socket(strctP->ai_family, strctP->ai_socktype, strctP->ai_protocol)) == -1){
			std::cout<<"Socker error\n";
			error();
			continue;
		}
		if(connect(sckSocket, strctP->ai_addr, strctP->ai_addrlen) == -1){
			std::cout<<"Error connecting\n";
			error();
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
	std::vector<std::string> vcUrl, vcUrl2;
	SplitString(cUrl, '/', vcUrl, 50); //increase this for larger subdirectories or url
	if(vcUrl.size() < 2){
		#ifdef _DEBUG
		std::cout<<"Unable to parse url "<<cUrl<<'\n';
		#endif
		return false;
	}
	
	unsigned long int uliFileSize = 0, uliResponseTotalBytes = 0, uliBytesSoFar = 0;
	int iErr = 0, iBytesWrited = 0, iLen = 0, iBytesReaded = 0;
	char cBuffer[2049], cFileBuffer[2049], cHostname[128], cTmp[128], cRemotePort[7];
	memset(cRemotePort, 0, 7);
	char *cToken = nullptr;
	
	std::size_t iLocation = 0, iNLocation = 0, HeaderEnd = 0;
	std::string strTmpFileName = vcUrl[vcUrl.size()-1], strTmpResponse = "", strTmp = "";
	std::ofstream sFile;
	
	SSL *ssl = nullptr;
	
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
	iLen = strHeader.length();
	strncpy(cTmp, vcUrl[2].c_str(), 127);
	cToken = strtok(cTmp, ":");
	if(cToken != nullptr){
		strncpy(cHostname, cToken, 127);
		cToken = strtok(nullptr, ":");
		if(cToken != nullptr){
			strncpy(cRemotePort, cToken, 6);
		}
	}
	
	if(strlen(cHostname) == 0){
		//No port provided on url
		strncpy(cHostname, vcUrl[2].c_str(), 127);
	}
	
	if(strlen(cRemotePort) == 0){
		if(isSSL){
			strncpy(cRemotePort, "443", 4);
		} else {
			strncpy(cRemotePort, "80", 3);
		}
	}
	
	while(1){ //loop until receive 200 ok to procede download
		if(InitSocket(cHostname, cRemotePort) == -1){
			bFlag = false;
			break;
		}
		if(isSSL){
			ssl = SSL_new(SSL_CTX_new(TLS_method()));
			if(ssl == nullptr){
				std::cout<<"Error creating ssl object\n";
				error();
				bFlag = false;
				break;
			}
			SSL_set_fd(ssl, sckSocket);
			iErr = SSL_connect(ssl);
			if(iErr == -1){
				std::cout<<"Error establishing SSL connection\n";
				error();
				bFlag = false;
				break;
			}
			iBytesWrited = SSL_write(ssl, (const char *)strHeader.c_str(), iLen);
		} else {
			iBytesWrited = send(sckSocket, strHeader.c_str(), iLen , 0);
		}
		
		if(iBytesWrited > 0){
			memset(cBuffer, 0, 2049);
			if(isSSL){
				iBytesReaded = SSL_read(ssl, cBuffer, 2048);
			} else {
				iBytesReaded = recv(sckSocket, cBuffer, 2048, 0);
			}
			
			if(iBytesReaded > 0){
				cBuffer[iBytesReaded] = '\0';
				strTmpResponse = cBuffer;
				if(strTmpResponse.find("HTTP/1.1 200 ") != std::string::npos || strTmpResponse.find("HTTP/1.0 200 ") != std::string::npos){
					memcpy(cFileBuffer, cBuffer, iBytesReaded);
					iLocation = strTmpResponse.find("filename=");
					if(iLocation != std::string::npos){
						iNLocation = strTmpResponse.find("\r", iLocation);
						if(iNLocation != std::string::npos){
							strTmp = strTmpResponse.substr(iLocation +9, (iNLocation - iLocation) - 9);
						}
					}
					if(uliFileSize == 0){
						iLocation = strTmpResponse.find("Content-Length: ");
						if(iLocation != std::string::npos){
							iNLocation = strTmpResponse.find('\r', iLocation);
							if(iNLocation != std::string::npos){
								strTmp = strTmpResponse.substr(iLocation + 16, (iNLocation - iLocation) - 16);
								uliFileSize = StrToint(strTmp.c_str());
								uliResponseTotalBytes = uliFileSize + HeaderEnd;
								std::cout<<"File size is "<<uliFileSize<<'\n';
							}
						}
					}
					if(uliFileSize == 0){
						std::cout<<"Unable to retrieve remote filesize\n";
						bFlag = false;
						break;
					}
					
					//save previous part of file that has been downloaded
					sFile.open(strTmpFileName, std::ios::binary);
					if(!sFile.is_open()){
						std::cout<<"Unable to open file "<<strTmpFileName<<"\nOpening dummy file /tmp/dowload.t3mp\n";
						error();
						sFile.open("/tmp/download.t3mp", std::ios::binary);
						if(!sFile.is_open()){
							std::cout<<"Unable to open dummy file\n";
							error();
							close(sckSocket);
							sckSocket = -1;
							bFlag = false;
							break;
						}
					}
					HeaderEnd = std::string(cBuffer).find("\r\n\r\n") + 4;
					sFile.write(&cFileBuffer[HeaderEnd], iBytesReaded - HeaderEnd);
					uliBytesSoFar = iBytesReaded;
					while(1){
						if(isSSL){
							iBytesReaded = SSL_read(ssl, cFileBuffer, 1024);
						} else {
							iBytesReaded = recv(sckSocket, cFileBuffer, 1024, 0);
						}
						if(iBytesReaded > 0){
							sFile.write(cFileBuffer, iBytesReaded);
							uliBytesSoFar += iBytesReaded;
							ProgressBar(uliBytesSoFar, uliResponseTotalBytes);
							std::fflush(stdout);
							if(uliBytesSoFar>=uliResponseTotalBytes){
								bFlag = true;
								goto releaseSSL;
							}
						} else {
							if(isSSL){
								switch(SSL_get_error(ssl, iBytesReaded)){
									case SSL_ERROR_NONE:
										continue;
										break;
									case SSL_ERROR_ZERO_RETURN:
										std::cout<<"SSL_ERROR_ZERO_RETURN\n";
										error();
										bFlag = false;
										iErr = 1;
										break;
									case SSL_ERROR_WANT_READ:
										std::cout<<"SSL_ERROR_WANT_READ\n";
										error();
										bFlag = false;
										iErr = 1;
										break;	
								}
							}
							if(iErr == 1){
								break;
							}
							if (iBytesReaded == -1) {
								std::cout<<"Connection closed\n";
								error();
								bFlag = false;
								goto releaseSSL;
							}
						}
					}
				} else if(strTmpResponse.find("HTTP/1.1 301 ") != std::string::npos){
					//follow redirection
					iLocation = strTmpResponse.find("Content-Length: ");
					if(iLocation != std::string::npos){
						iNLocation = strTmpResponse.find('\r', iLocation);
						if(iNLocation != std::string::npos){
							strTmp = strTmpResponse.substr(iLocation + 16, (iNLocation - iLocation) - 16);
							uliFileSize = StrToint(strTmp.c_str());
							std::cout<<"File size is "<<uliFileSize<<'\n';
						}
					}
					iLocation = std::string(cBuffer).find("Location: ");
					if(iLocation != std::string::npos){
						iNLocation = std::string(cBuffer).find('\r', iLocation);
						if(iNLocation != std::string::npos){
							if(sckSocket){
								close(sckSocket);
								sckSocket = -1;
							}
							if(isSSL){
								SSL_free(ssl);
							}
							strTmp = std::string(cBuffer).substr(iLocation +10, iNLocation - iLocation - 10);
							std::cout<<"Redirected to "<<strTmp<<'\n';
							SplitString(strTmp.c_str(), '/', vcUrl2, 50);
							if(vcUrl2.size() < 2){
								std::cout<<"Unable to parse url: "<<strTmp<<'\n';
								bFlag = false;
								break;
							}
							if(vcUrl2[0] == "https:"){
								isSSL = true;
							} else {
								isSSL = false;
							}
							strHeader.erase(strHeader.begin(), strHeader.end());
							strHeader = "GET ";
							for(int iIt3 = 3; iIt3<int(vcUrl2.size()); iIt3++){
								strHeader.append(1, '/');
								strHeader.append(vcUrl2[iIt3]);
							}
							strHeader.append(" HTTP/1.1\r\nHost: ");
							strHeader.append(vcUrl2[2]);
							strHeader.append("\r\nConnection: Keep-Alive\r\n\r\n");
							memset(cTmp, 0, 128);
							strncpy(cTmp, vcUrl2[2].c_str(), 127);
							cToken = strtok(cTmp, ":");
							if(cToken != nullptr){
								memset(cHostname, 0, 128);
								memset(cRemotePort, 0, 7);
								strncpy(cHostname, cToken, 127);
								cToken = strtok(nullptr, ":");
								if(cToken != nullptr){
										strncpy(cRemotePort, cToken, 6);
								}
							}
							if(strlen(cHostname) == 0){
								strncpy(cHostname, vcUrl2[2].c_str(), 127);
							}
							if(strlen(cRemotePort) == 0){
									if(isSSL){
										strncpy(cRemotePort, "443", 4);
									} else {
										strncpy(cRemotePort, "80", 3);
									}
							}
							strTmpFileName.erase(strTmpFileName.begin(), strTmpFileName.end());
							strTmpFileName = vcUrl2[vcUrl2.size()-1];
							continue;
						} else {
							std::cout<<"Unable to parse new location\n";
							bFlag = false;
							break;
						}
					} else {
						std::cout<<"Unable to parse new location\n";
						bFlag = false;
						break;
					}
				} else {
					std::size_t pos = std::string(cBuffer).find("\r\n");
					std::string strTmp = std::string(cBuffer).substr(0, pos);
					std::cout<<"Not handled response code\n"<<strTmp<<'\n';
					bFlag = false;
					break;
				}
			} else {
				std::cout<<"Got no response from server\n";
				error();
				bFlag = false;
				break;
			}
		} else {
			std::cout<<"Unable to send packet\n";
			error();
			bFlag = false;
			break;
		}
		
	}
	
	releaseSSL:
	if(sckSocket){
		close(sckSocket);
		sckSocket = -1;
	}
	if(isSSL){
		if(ssl != nullptr){
			SSL_free(ssl);
		}
	}
	return bFlag;
}


int main(int argc, char **argv){
	if(argc != 2){
		std::cout<<"Use "<<argv[0]<<" url\n";
		return 0;
	}
	Downloader esta;
	if(esta.Download(argv[1])){
		std::cout<<"Download success\n";
	}
	return 0;
}
