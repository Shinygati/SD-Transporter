#include "../retail/arm9/include/walker_history.h"
#include <assert.h>
#include <filesystem>
#include <vector>
#include <chrono>
namespace fs=std::filesystem;
static void write(const fs::path& p,const std::vector<unsigned char>& data){
    FILE *f=fopen(p.string().c_str(),"wb");assert(f);
    assert(fwrite(data.data(),1,data.size(),f)==data.size());assert(!fclose(f));
}
int main(){
    fs::path root=fs::temp_directory_path()/"walker-history-test"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(root);
    auto source=root/"test.walker";
    std::vector<unsigned char> a(65536),b;
    for(unsigned i=0;i<a.size();i++)a[i]=(i*31+7)&255;
    write(source,a);assert(walkerPrelaunchSnapshot(source.string()));
    auto backups=root/"transfers/backups";
    fs::path first;unsigned count=0;
    for(auto& p:fs::directory_iterator(backups)){first=p.path();count++;}
    assert(count==1 && walkerHistoryMatches(first.string(),a.data()));
    assert(walkerPrelaunchSnapshot(source.string()));
    count=0;for(auto& p:fs::directory_iterator(backups))count++;assert(count==1);
    b=a;b[123]^=7;write(source,b);assert(walkerPrelaunchSnapshot(source.string()));
    assert(walkerHistoryMatches(first.string(),a.data()));
    count=0;for(auto& p:fs::directory_iterator(backups))count++;assert(count==2);
    write(first,std::vector<unsigned char>{1});write(source,a);
    assert(!walkerPrelaunchSnapshot(source.string()));assert(fs::file_size(first)==1);
    write(source,std::vector<unsigned char>(65535));assert(!walkerPrelaunchSnapshot(source.string()));
    auto blocked=root/"blocked";fs::create_directory(blocked);
    write(blocked/"transfers",std::vector<unsigned char>{1});write(blocked/"test.walker",a);
    assert(!walkerPrelaunchSnapshot((blocked/"test.walker").string()));
    assert(walkerHistoryMatches(source.string(),a.data())==false);
    auto second=root/"other.walker";
    for(unsigned n=0;n<14;n++){a[222]=(unsigned char)n;write(source,a);write(second,a);assert(walkerPrelaunchSnapshot(source.string()));assert(walkerPrelaunchSnapshot(second.string()));}
    unsigned one=0,two=0;for(auto& p:fs::directory_iterator(backups)){auto name=p.path().filename().string();if(fs::file_size(p)==65536){if(name.find("test.walker.")==0)one++;if(name.find("other.walker.")==0)two++;}}
    assert(one==10&&two==10);
    puts("Pre-launch retention: ten snapshots per walker, separate histories preserved.");
    puts("Pre-launch backups: byte verification, immutable retries, changed walker preservation, corrupt-copy refusal, short-file refusal and blocked-folder refusal passed.");
}
