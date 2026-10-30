#pragma once
#include <stdint.h>

// OFF/ON state machine; persistent daily limits are enforced by RestartBudget.
struct EssRestart {
    enum Phase {Idle,WaitOff,OffDelay,WaitOn,Done,Aborted};
    enum Action {None,SendOff,SendOn,Stop};
    Phase phase=Idle;
    bool used=false,tracking=false;
    uint32_t since=0,lastObservation=0,phaseMs=0,onMs=0;
    bool active()const{return phase==WaitOff||phase==OffDelay||phase==WaitOn;}
    void cancel(){if(active())phase=Aborted;tracking=false;}
    bool observe(uint32_t now,bool eligible,bool delivering){
        if(active()||!eligible||delivering){tracking=false;return false;}
        if(!tracking||now-lastObservation>7500){since=now;tracking=true;}
        lastObservation=now;
        if(now-since<120000)return false;
        used=true;phase=WaitOff;phaseMs=now;tracking=false;return true;
    }
    Action advance(uint32_t now,bool safe,bool offConfirmed,bool onConfirmed,bool newSamples){
        if(!active())return None;
        if(!safe){const bool wasOn=phase==WaitOn;phase=Aborted;return wasOn?SendOff:Stop;}
        if(phase==WaitOff){
            if(offConfirmed){phase=OffDelay;phaseMs=now;}
            else if(now-phaseMs>=20000){phase=Aborted;return Stop;}
        }else if(phase==OffDelay&&now-phaseMs>=10000){
            if(!offConfirmed){phase=Aborted;return Stop;}
            phase=WaitOn;phaseMs=onMs=now;return SendOn;
        }else if(phase==WaitOn){
            if(onConfirmed&&newSamples){phase=Done;return None;}
            if(now-phaseMs>=20000){phase=Aborted;return SendOff;}
        }
        return None;
    }
    const char *status()const{
        switch(phase){case WaitOff:return "ESS-Neustart: Aus angefordert";
        case OffDelay:return "ESS-Neustart: 10 Sekunden ausgeschaltet warten";
        case WaitOn:return "ESS-Neustart: Ein angefordert, warte auf Messwerte";
        case Done:return "Aus-/Ein-Zyklus abgeschlossen, Wirkung prüfen";
        case Aborted:return "Aus-/Ein-Zyklus abgebrochen, manuell prüfen";
        default:return "Noch kein Aus-/Ein-Zyklus";}
    }
};
