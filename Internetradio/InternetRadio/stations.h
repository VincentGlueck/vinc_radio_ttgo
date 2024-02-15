struct Station {
  char name[12];  // longer names will be truncated on screen (you may increase to 24 or whatever, but use smaller font)
  char url[96];
  float gain;  // inital gain (1.0f - 15.0f)
};

// not sure, but streams ending with .m3u do crash device often :-()
const Station stations[] = {
  { "NDR Enjoy", "http://icecast.ndr.de/ndr/njoy/live/mp3/128/stream.mp3", 10.0f },
  { "Suns LIVE", "http://stream.sunshine-live.de/live/mp3-192", 8.0f },
  { "Suns House", "http://stream.sunshine-live.de/house/mp3-192", 8.0f },
  { "Suns Lounge", "http://stream.sunshine-live.de/lounge/mp3-192", 8.0f },
  { "NDR 2", "http://icecast.ndr.de/ndr/ndr2/hamburg/mp3/128/stream.mp3", 10.0f },
  { "NDR 1 NDS", "http://icecast.ndr.de/ndr/ndr1niedersachsen/hannover/mp3/128/stream.mp3", 10.0f },
  { "1LIVE", "http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3", 10.0f },
  { "VRT Pop", "http://icecast.vrtcdn.be/ra2ant-high.mp3", 8.0f },
  { "Danmarks", "http://live-icy.gss.dr.dk:8000/A/A08H.mp3", 8.0f },
  { "Eldorado", "http://sender.eldoradio.de:8000/128.mp3", 8.0f },
  { "FluxFM", "http://streams.fluxfm.de/klubradio/mp3-128/audio/", 8.0f },
  { "JamFM", "http://stream.jam.fm/jamfm-live/mp3-128", 8.0f },
  { "KissFM", "http://topradio-de-hz-fal-stream09-cluster01.radiohost.de/kissfm_128", 8.0f },
  { "Sveriges", "http://sverigesradio.se/topsy/direkt/701-hi-mp3", 8.0f },
  { "Energy SUI", "http://energyzuerich.ice.infomaniak.ch/energyzuerich-high.mp3", 8.0f }
};