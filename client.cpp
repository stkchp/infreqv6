#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <net/if.h>

#include <array>
#include <cstring>
#include <iostream>
#include <vector>
#include <random>

constexpr int NEVENTS = 16;
constexpr auto SRC_PORT = 546;
constexpr auto DST_PORT = 547;
constexpr char DST_ADDR[] = "ff02::1:2";

namespace {
	class worker {
		public:
			int fd;
			std::array<char, 6> lladdr;
			std::array<char, 3> txid;
			unsigned int ifindex;
			worker() : fd(-1), lladdr({0}), txid({0}), ifindex(0) {}
			~worker() {
				if(fd >= 0) close(fd);
			}
			bool check(char *ifname) {
				// get interface index
				ifindex = if_nametoindex(ifname);
				if(ifindex == 0) {
					std::cerr << "Error: if_nametoindex() failed." << std::endl;
					return false;
				}

				// get interface mac address
				struct ifreq s;
				int tmpfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
				strcpy(s.ifr_name, ifname);
				if (ioctl(tmpfd, SIOCGIFHWADDR, &s) == 0) {
					for (int i = 0; i < 6; ++i) {
						lladdr.at(i) = s.ifr_addr.sa_data[i];
					}
					return true;
				}

				std::cerr << "Error: cannot get mac address." << std::endl;
				return false;
			}

			bool create() {
				fd = socket(AF_INET6, SOCK_DGRAM, 0);
				if (fd < 0) {
					std::cerr << "Error: socket() failed." << std::endl;
					return false;
				}

				// set broadcast interface
				if(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)) != 0) {
					std::cerr << "Error: setsockopt(IPV6_MULTICAST_IF) failed." << std::endl;
					return false;
				}

				// bind port
				struct sockaddr_in6 addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin6_family = AF_INET6;
				addr.sin6_port = htons(SRC_PORT);
				addr.sin6_addr = in6addr_any;
				addr.sin6_scope_id = ifindex;
				int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
				if (ret < 0) {
					std::cerr << "Error: bind() failed." << std::endl;
					return false;
				}
				
				return true;
			}
			std::array<char, 3> build_txid() {
				// get random 3byte id
				std::array<char, 3> buf = {0x00, 0x00, 0x00};
				std::random_device rnd;
				std::mt19937 mt(rnd());
				std::uniform_int_distribution<> rand256(0x00, 0xff);
				for(size_t i = 0; i < buf.size(); ++i) {
					buf.at(i) = (rand256(mt) & 0xff);
				}
				return buf;
			}
			std::array<char, 14> build_option_client_id() {
				std::array<char, 14> buf = {
					0x00, 0x01, // option: client id
					0x00, 0x0a, // length
					0x00, 0x03, // duid: link-layer address
					0x00, 0x01, // type: ethernet
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // mac address
				};
				// fill mac address
				std::copy(lladdr.begin(), lladdr.end(), buf.begin() + 8); 
				return buf;
			}
			std::array<char, 6> build_option_request() {
				std::array<char, 6> buf = {
					0x00, 0x06, // option: request
					0x00, 0x02, // length
					0x00, 0x17  // DNS recursive name server
				};
				return buf;
			}
			std::vector<char> build_packet() {
				std::vector<char> buf = {0x0b};
				
				// append transaction id
				txid = build_txid();
				buf.insert(buf.end(), txid.begin(), txid.end());

				// append option client id
				auto clid = build_option_client_id();
				buf.insert(buf.end(), clid.begin(), clid.end());

				// append option request
				auto req = build_option_request();
				buf.insert(buf.end(), req.begin(), req.end());
				
				return buf;
			}
			bool send_packet() {
				auto buf = build_packet();

				// destination info
				struct sockaddr_in6 addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin6_family = AF_INET6;
				addr.sin6_port = htons(DST_PORT);
				addr.sin6_scope_id = ifindex;
				if(inet_pton(AF_INET6, DST_ADDR, &addr.sin6_addr) != 1) {
					std::cerr << "Error: inet_pton() failed." << std::endl;
					return false;
				}

				// send
				auto ret = sendto(fd, buf.data(), buf.size(), 0, (struct sockaddr *)&addr, sizeof(addr));
				if(ret <= 0) {
					std::cerr << "Error: sendto() failed. errno=" << errno << std::endl;
					std::cerr << "       ret = " << ret << std::endl;
					std::cerr << "       buf.size() = " << buf.size() << std::endl;
					return false;
				}
				
				return true;
			}
			std::vector<char> get_packet() {
				std::vector<char> buf(4096);
				auto r = recv(fd, buf.data(), buf.size(), 0);
				if(r < 0) {
					std::cerr << "Error: recv() failed." << std::endl;
					buf.resize(0);
				} else {
					buf.resize(r);
				}
				return buf;
			}

			bool recv_packet() {
				auto buf = get_packet();
				if(buf.size() <= 4) {
					std::cerr << "Error: get_packet() failed." << std::endl;
					return false;
				}
				
				char mtype = buf.at(0);
				std::array<char, 3> rtxid;
				std::copy(buf.begin() + 1, buf.begin() + 4, rtxid.begin());
				// return check
				if(mtype != 0x07 || txid != rtxid) {
					std::cerr << "Error: Transacsion ID not matched." << std::endl;
					return false;
				}
				
				size_t i = 4;
				while(i + 4 <= buf.size()) {
					uint16_t op = ((uint16_t)buf.at(i) << 8) | (uint16_t) buf.at(i + 1);
					uint16_t len = ((uint16_t)buf.at(i + 2) << 8) | (uint16_t) buf.at(i + 3);
					i += 4;
					std::vector<char> data;
					data.insert(data.end(), buf.begin() + i, buf.begin() + i + len);
					check_option_data(op, len, data);
					i += len;
				}
				return true;
			}
			void check_option_data(uint16_t op, uint16_t len, std::vector<char> &data) {
				// DNS recursive name server
				if(op == 23) {
					for(uint16_t i = 0; i < data.size(); i += 16) {
						std::array<char, 40> buf = {0};
						auto r = inet_ntop(AF_INET6, &data.at(i), buf.data(), buf.size());
						if(r != NULL) {
							std::cout << buf.data() << std::endl;
						}
					}
				}
			}
	};
	class poller {
		public:
			int efd;
			int sfd;
			poller() : efd(-1), sfd(-1) {}
			~poller() {
				if(sfd >= 0) close(sfd);
				if(efd >= 0) close(efd);
			}

			bool setfd(int fd) {
				struct epoll_event ev;
				ev.data.fd = fd;
				ev.events = EPOLLIN;
				if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
					std::cerr << "Error: epoll_ctl(add) failed." << std::endl;
					return false;
				}

				return true;
			}
			bool setup() {
				// epollfd 作成
				efd = epoll_create(NEVENTS);
				if (efd == -1) {
					std::cerr << "Error: epoll_create() failed." << std::endl;
					return false;
				}

				// signalfd 作成
				sigset_t mask;
				sigemptyset(&mask);
				sigaddset(&mask, SIGINT);
				sigaddset(&mask, SIGPIPE);
				sigaddset(&mask, SIGTERM);
				sigprocmask(SIG_BLOCK, &mask, NULL);
				sfd = signalfd(-1, &mask, 0);
				if (sfd == -1) {
					std::cerr << "Error: signalfd() failed." << std::endl;
					return false;
				}
				if(!setfd(sfd)) return false;
				return true;
			}
	};
}

int main(int argc, char *argv[]) {

	// check argument
	if(argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <ifname>" << std::endl;
		return 1;
	}

	char *ifname = argv[1];

	worker w;
	poller p;

	if(!w.check(ifname)) {
		std::cerr << "Error: check() failed." << std::endl;
		return 2;
	}
	if(!w.create()) {
		std::cerr << "Error: create() failed." << std::endl;
		return 3;
	}
	if(!p.setup()) {
		std::cerr << "Error: setup() failed." << std::endl;
		return 4;
	}
	if(!p.setfd(w.fd)) {
		std::cerr << "Error: setfd() failed." << std::endl;
		return 5;
	}

	// send packet
	if(!w.send_packet()) {
		std::cerr << "Error: send_packet() failed." << std::endl;
		return 6;
	}

	/* epoll create */
	struct epoll_event evs[NEVENTS];

	// 5s timeout
	auto nfds = epoll_wait(p.efd, evs, NEVENTS, 5000);

	if (nfds < 0) {
		std::cerr << "Error: epoll_wait() failed." << std::endl;
		return 7;
	}
	
	if (nfds == 0) {
		std::cerr << "Error: epoll_wait() timeout." << std::endl;
		return 8;
	}

	for (int i = 0; i < nfds; i++) {
		int fd = evs[i].data.fd;
		if (fd == w.fd) {
			if(!w.recv_packet()) {
				std::cerr << "Error: recv_packet() failed." << std::endl;
				return 9;
			}
		}
		else if(fd == p.sfd) {
			struct signalfd_siginfo info = {0};
			read(p.sfd, &info, sizeof(info));
			std::cerr << std::endl << "Info: catch signal." << std::endl;
			return 10;
		}
	}

	return 0;
}

