# ESP32_GPS_Tracker_GSM
ESP32 + SIM7000 GPS/4g modem + thingsboard

ESP32 gets GPS data from SIM7000 and will attempt to publish it to thingboard.io over WIFI if it is within a geofence, otherwise, it will try to do the same over cellular.  Couldn't find any Wifi -> cellular failover projects so posted this.  I hacked together examples from the referenced libraries to come up with this.  
