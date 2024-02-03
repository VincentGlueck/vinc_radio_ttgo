struct Station {
  char name[24];
  char url[96];
  float gain;
};

const Station stations[] = {
  {"NDR Enjoy", "http://icecast.ndr.de/ndr/njoy/live/mp3/128/stream.mp3", 4.0f},
  {"Sunshine LIVE", "http://stream.sunshine-live.de/live/mp3-192", 3.0f},
  {"Sunshine House", "http://stream.sunshine-live.de/house/mp3-192", 3.0f},
  {"Sunshine Lounge", "http://stream.sunshine-live.de/lounge/mp3-192", 3.0f},
  {"NDR 2", "http://icecast.ndr.de/ndr/ndr2/hamburg/mp3/128/stream.mp3", 4.0f},
  {"NDR 1 NDS", "http://icecast.ndr.de/ndr/ndr1niedersachsen/hannover/mp3/128/stream.mp3", 4.0f},
  {"1LIVE", "http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3", 4.0f}
};