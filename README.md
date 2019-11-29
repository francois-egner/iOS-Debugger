Basierend auf der Ausarbeitung für die Projektgruppe "IT-Sicherheit" an der Rheinischen Friedrich-Wilhelms-Universität in Bonn.  
Ausarbeitung zu finden auf: https://www.francois-egner.de/projekte ("Entwicklung eines iOS-Debuggers")  
Abschlusspräsentation der Projektgruppe: https://bit.ly/2L6bEaO  

Nutzung:
----------------------------
Speicher beschreiben: memory write [0xAdresse] [0xDaten]  
Speicher lesen: memory read [0xAdresse]  
Register lesen: register read [index]  
Register beschreiben: register set [index] [0xDaten]  
Alle Register anzeigen: register showAll  
Softwarebreakpoint setzen: breakpoint set [0xAdresse]  
Softwarebreakpoint löschen: breakpoint delete [0xAdresse]  
Alle Breakpoints anzeigen: breakpoint showAll  
Single Step: n oder next  
Fortsetzen: c oder continue  

Implementiert:  
----------------------------
Task pausieren & fortsetzen  
Speicher lesen & schreiben  
Register lesen & schreiben  
Breakpoint setzen & löschen  
Single Step  
  

Ggf. noch zu implementieren:  
----------------------------
Codesegment -> Assembly  
