#include <pthread.h>

#include "bitcoin.h"
#include "db.h"

#define NTHREADS 100

using namespace std;

extern "C" {
#include "dns.h"
}

CAddrDb db;

extern "C" void* ThreadCrawler(void* data) {
  do {
    db.Stats();
    CIPPort ip;
    int wait = 5;
    if (!db.Get(ip, wait)) {
      wait *= 1000;
      wait += rand() % (500 * NTHREADS);
      Sleep(wait);
      continue;
    }
    int ban = 0;
    vector<CAddress> addr;
    bool ret = TestNode(ip,ban,addr);
    db.Add(addr);
    if (ret) {
      db.Good(ip);
    } else {
      db.Bad(ip, ban);
    }
  } while(1);
}

extern "C" int GetIPList(struct in_addr *addr, int max, int ipv4only) {
  set<CIP> ips;
  db.GetIPs(ips, max, ipv4only);
  int n = 0;
  for (set<CIP>::iterator it = ips.begin(); it != ips.end(); it++) {
    if ((*it).GetInAddr(&addr[n]))
      n++;
  }
  return n;
}

extern "C" void* ThreadDNS(void*) {
  dns_opt_t opt;
  opt.host = "seedtest.bitcoin.sipa.be";
  opt.ns = "vps.sipa.be";
  opt.mbox = "sipa.ulyssis.org";
  opt.datattl = 60;
  opt.nsttl = 40000;
  opt.cb = GetIPList;
  opt.port = 53;
  dnsserver(&opt);
}

extern "C" void* ThreadDumper(void*) {
  do {
    Sleep(100000);
    {
      FILE *f = fopen("dnsseed.dat","w+");
      if (f) {
        CAutoFile cf(f);
        cf << db;
      }
    }
  } while(1);
}

#define NTHREADS 100

int main(void) {
  FILE *f = fopen("dnsseed.dat","r");
  if (f) {
    CAutoFile cf(f);
    cf >> db;
  }
  vector<CIP> ips;
  LookupHost("dnsseed.bluematt.me", ips);
  for (vector<CIP>::iterator it = ips.begin(); it != ips.end(); it++) {
    db.Add(CIPPort(*it, 8333));
  }
  pthread_t thread[NTHREADS+2];
  for (int i=0; i<NTHREADS; i++) {
    pthread_create(&thread[i], NULL, ThreadCrawler, NULL);
  }
  pthread_create(&thread[NTHREADS], NULL, ThreadDumper, NULL);
  pthread_create(&thread[NTHREADS+1], NULL, ThreadDNS, NULL);
  for (int i=0; i<NTHREADS+2; i++) {
    void* res;
    pthread_join(thread[i], &res);
  }
  return 0;
}
