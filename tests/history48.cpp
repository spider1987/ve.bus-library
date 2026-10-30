#include "../examples/mqtt_ha/History48.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
using namespace History48;
Sample sample(){Sample s={{100,-250,300,4950,570,-65}};return s;}
struct BufferedFile {
 FILE *f;
 bool seek(uint32_t n){return std::fseek(f,n,SEEK_SET)==0;}
 size_t write(const uint8_t *p,size_t n){return std::fwrite(p,1,n,f);}
};
int main(){
 // Reproduce VFS stat lag after a buffered 20-byte header write.
 FILE *file=std::tmpfile();assert(file);char buffer[4096];std::setvbuf(file,buffer,_IOFBF,sizeof(buffer));
 auto durableHeader=header();assert(std::fwrite(&durableHeader,1,sizeof(durableHeader),file)==sizeof(durableHeader));
 struct stat before={};assert(fstat(fileno(file),&before)==0);assert(before.st_size==0);
 BufferedFile buffered{file};assert(!padFile(buffered,0)); // refuse stale metadata length
 assert(padFile(buffered,sizeof(Header)));std::fflush(file);
 std::rewind(file);Header after={};assert(std::fread(&after,1,sizeof(after),file)==sizeof(after)&&valid(after));
 struct stat size={};assert(fstat(fileno(file),&size)==0&&(uint32_t)size.st_size==fileBytes());std::fclose(file);

 assert(CAPACITY==1440&&fileBytes()==28820);
 Accumulator a;a.add(0,100);a.add(0,200);a.add(3,4950);a.add(4,30000);a.add(4,std::numeric_limits<float>::quiet_NaN());auto avg=a.finish();assert(avg.value[0]==150&&avg.value[3]==4950&&avg.value[4]==GAP&&avg.value[1]==GAP);assert(empty(a.finish()));
 auto headerValue=header();assert(valid(headerValue));headerValue.version++;assert(!valid(headerValue));
 auto bad=sample();bad.value[4]=30000;assert(!valid(bad));
 const uint32_t now=MIN_MINUTE+10000;Window window;assert(!window.get(0));
 assert(window.put({now-5,sample()}));assert(window.put({now,sample()}));assert(window.first()==now-5&&!window.get(now-4));assert(window.count()==2);
 assert(!window.put({now-CAPACITY,sample()}));
 for(uint32_t i=0;i<CAPACITY+10;++i)assert(window.put({now+i,sample()}));
 assert(window.count()==CAPACITY&&window.first()==now+10&&!window.get(now+9));
 assert(inWindow(now,now)&&!inWindow(now+1,now)&&!inWindow(now-CAPACITY,now));
 DiskRecord r=record({now,sample()});assert(valid(r));r.point.sample.value[0]++;assert(!valid(r));
 const char *path=".pio/build/history48-test.bin";
 {std::ofstream file(path,std::ios::binary|std::ios::trunc);auto h=header();file.write((char*)&h,sizeof(h));DiskRecord zeros={};for(uint32_t i=0;i<CAPACITY;++i)file.write((char*)&zeros,sizeof(zeros));assert((uint32_t)file.tellp()==fileBytes());}
 {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);for(uint32_t minute=now-10;minute<=now;++minute){if(minute==now-4)continue;auto row=record({minute,sample()});file.seekp(offset(minute));file.write((char*)&row,sizeof(row));}}
 Window reboot;
 {std::ifstream file(path,std::ios::binary);Header h={};file.read((char*)&h,sizeof(h));assert(valid(h));for(uint32_t i=0;i<CAPACITY;++i){DiskRecord row={};file.read((char*)&row,sizeof(row));if(valid(row)&&inWindow(row.point.minute,now))reboot.put(row.point);}}
 assert(reboot.count()==10&&reboot.latest==now&&!reboot.get(now-4)&&reboot.get(now-5));
 // Interrupted replacement of the same ring slot must fail its record checksum.
 {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);uint32_t partial=now+CAPACITY;file.seekp(offset(partial));file.write((char*)&partial,sizeof(partial));}
 {std::ifstream file(path,std::ios::binary);DiskRecord torn={};file.seekg(offset(now));file.read((char*)&torn,sizeof(torn));assert(!valid(torn));}
 std::cout<<"History48: 24-hour ring, restart replay, real gaps, expiry/future rejection, CRC/torn records and per-field SoC validation OK\n";
}
