#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include "History48.h"

// Internal flash only. The SD slot and SPI pins are never initialized.
// Only this worker owns LittleFS. Unknown filesystems are never formatted.
class FlashHistory {
public:
    enum State:uint32_t {Starting,Missing,WaitingClock,Restoring,Ready,IoError,FormatError,TaskError};
    volatile uint32_t state=Starting,written=0,writeErrors=0,readErrors=0,lastMinute=0,recovered=0,totalBytes=0,usedBytes=0;
    uint32_t dropped=0;
    void begin(){
        writes=xQueueCreate(8,sizeof(History48::Point));restores=xQueueCreate(16,sizeof(History48::Point));
        if(!writes||!restores){if(writes)vQueueDelete(writes);if(restores)vQueueDelete(restores);writes=restores=nullptr;state=TaskError;return;}
        if(xTaskCreatePinnedToCore(task,"flash-history",8192,this,1,nullptr,0)!=pdPASS){vQueueDelete(writes);vQueueDelete(restores);writes=restores=nullptr;state=TaskError;}
    }
    void enqueue(const History48::Point &p){if(!writes||xQueueSend(writes,&p,0)!=pdTRUE)++dropped;}
    bool take(History48::Point &p){return restores&&xQueueReceive(restores,&p,0)==pdTRUE;}
    const char *status() const {
        switch(state){case Starting:return "Flash-Verlauf wird initialisiert";case Missing:return "Flash nicht eingebunden · Verlauf nur im RAM";
        case WaitingClock:return "Flash bereit · warte auf Zeitabgleich";case Restoring:return "Gespeicherter Verlauf wird geladen";
        case Ready:return "Flash-Aufzeichnung aktiv · 24 Stunden";case IoError:return "Flash-Schreib-/Lesefehler · Verlauf im RAM";
        case FormatError:return "Unbekanntes Verlaufsformat · Datei unverändert";default:return "Flash-Aufgabe nicht gestartet · Verlauf im RAM";}
    }
private:
    QueueHandle_t writes=nullptr,restores=nullptr;
    static uint32_t minuteNow(){time_t t=time(nullptr);return t>=1704067200LL&&t<=4102444799LL?(uint32_t)(t/60):0;}
    static const char *path(){return "/vebus/history24-v1.bin";}
    static void task(void *arg){static_cast<FlashHistory*>(arg)->run();}
    const char *backupPath=nullptr;
    bool prepare(){
        if(!LittleFS.exists("/vebus")&&!LittleFS.mkdir("/vebus")){state=IoError;return false;}
        bool existing=LittleFS.exists(path());
        if(existing){
            File check=LittleFS.open(path(),FILE_READ);History48::Header h={};
            const bool valid=check&&check.read((uint8_t*)&h,sizeof(h))==sizeof(h)&&History48::valid(h)&&check.size()<=History48::fileBytes();
            check.close();
            if(!valid){
                static const char *backups[]={"/vebus/history24-bad-1.bin","/vebus/history24-bad-2.bin","/vebus/history24-bad-3.bin"};
                for(auto name:backups)if(!LittleFS.exists(name)){backupPath=name;break;}
                if(!backupPath||LittleFS.totalBytes()-LittleFS.usedBytes()<History48::fileBytes()+8192){state=FormatError;return false;}
                if(!LittleFS.rename(path(),backupPath)){state=IoError;return false;}
                existing=false;
            }
        }
        File f=LittleFS.open(path(),existing?"r+":FILE_WRITE);
        if(!f){state=IoError;return false;}
        uint32_t length=existing?f.size():0;
        if(!existing){
            const auto h=History48::header();
            if(f.write((const uint8_t*)&h,sizeof(h))!=sizeof(h)){f.close();state=IoError;return false;}
            // VFS size()/stat can lag buffered fwrite. Never seek to that stale size.
            f.flush();length=sizeof(h);
        }
        if(!History48::padFile(f,length)){f.close();state=IoError;return false;}
        f.flush();f.close();
        // Reopen and verify the durable header before accepting any samples.
        File check=LittleFS.open(path(),FILE_READ);History48::Header h={};
        const bool ok=check&&check.size()==History48::fileBytes()&&check.read((uint8_t*)&h,sizeof(h))==sizeof(h)&&History48::valid(h);
        check.close();if(!ok)state=IoError;return ok;
    }
    bool recoverBackup(){
        if(!backupPath)return true;
        File old=LittleFS.open(backupPath,FILE_READ);if(!old){state=IoError;return false;}
        if(!old.seek(sizeof(History48::Header))){old.close();return true;}
        // Salvage only CRC-validated records in the documented layout and window.
        const uint32_t limit=History48::CAPACITY*2;
        for(uint32_t i=0;i<limit;++i){
            History48::DiskRecord r={};if(old.read((uint8_t*)&r,sizeof(r))!=sizeof(r))break;
            if(History48::valid(r)&&History48::inWindow(r.point.minute,minuteNow())){
                if(!save(&r.point,1)){old.close();state=IoError;return false;}++recovered;
            }
            if(i%32==0)vTaskDelay(1);
        }
        old.close();return true;
    }
    bool restore(){
        state=Restoring;File f=LittleFS.open(path(),FILE_READ);if(!f){state=IoError;return false;}
        if(!f.seek(sizeof(History48::Header))){f.close();state=IoError;return false;}
        for(uint32_t i=0;i<History48::CAPACITY;++i){
            History48::DiskRecord r={};if(f.read((uint8_t*)&r,sizeof(r))!=sizeof(r)){++readErrors;f.close();state=IoError;return false;}
            if(History48::valid(r)){if(History48::inWindow(r.point.minute,minuteNow())){if(r.point.minute>lastMinute)lastMinute=r.point.minute;xQueueSend(restores,&r.point,portMAX_DELAY);}}
            else if(r.point.minute)++readErrors;
            if(i%64==0)vTaskDelay(1);
        }
        f.close();return true;
    }
    bool mount(){
        if(LittleFS.begin(false))return true;
        const esp_partition_t *part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
        if(!part)return false;
        uint8_t bytes[256];
        for(size_t offset=0;offset<part->size;offset+=sizeof(bytes)){
            if(esp_partition_read(part,offset,bytes,sizeof(bytes))!=ESP_OK)return false;
            for(auto b:bytes)if(b!=0xff)return false;
            vTaskDelay(1);
        }
        // Initialize only a completely erased partition, never an existing FS.
        return LittleFS.begin(true);
    }
    bool save(const History48::Point *points,unsigned count){
        File f=LittleFS.open(path(),"r+");if(!f)return false;
        bool ok=true;
        for(unsigned i=0;i<count&&ok;++i){const auto r=History48::record(points[i]);
            ok=f.seek(History48::offset(points[i].minute))&&f.write((const uint8_t*)&r,sizeof(r))==sizeof(r);
        }
        f.flush();
        for(unsigned i=0;i<count&&ok;++i){History48::DiskRecord check={};const auto expected=History48::record(points[i]);
            ok=f.seek(History48::offset(points[i].minute))&&f.read((uint8_t*)&check,sizeof(check))==sizeof(check)&&History48::valid(check)&&check.checksum==expected.checksum;
        }
        f.close();usedBytes=LittleFS.usedBytes();return ok;
    }
    void run(){
        if(!mount()){state=Missing;vTaskDelete(nullptr);return;}
        totalBytes=LittleFS.totalBytes();usedBytes=LittleFS.usedBytes();
        if(totalBytes-usedBytes<History48::fileBytes()+8192&&!LittleFS.exists(path())){state=IoError;vTaskDelete(nullptr);return;}
        if(!prepare()){vTaskDelete(nullptr);return;}
        state=WaitingClock;
        while(!minuteNow())vTaskDelay(pdMS_TO_TICKS(1000));
        if(!recoverBackup()||!restore()){vTaskDelete(nullptr);return;}
        state=Ready;usedBytes=LittleFS.usedBytes();
        History48::Point pending[5];unsigned count=0;uint32_t batchStarted=0;
        for(;;){
            History48::Point p;
            if(xQueueReceive(writes,&p,pdMS_TO_TICKS(250))==pdTRUE&&History48::inWindow(p.minute,minuteNow())){
                if(!count)batchStarted=millis();pending[count++]=p;
            }
            if(count&&(count==5||millis()-batchStarted>=300000)){
                if(save(pending,count)){written+=count;lastMinute=pending[count-1].minute;state=Ready;}
                else{writeErrors+=count;state=IoError;}
                count=0;
            }
        }
    }
};
