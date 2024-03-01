struct Station {
  char name[12];  // longer names will be truncated on screen (you may increase to 24 or whatever, but use smaller font)
  char url[96];
  float gain;  // inital gain (1.0f - 15.0f)
  bool isAAC;
};

// streams ending with .m3u do not work, aac = sometimes
const Station stations[] = {
  { "NDR Enjoy", "http://icecast.ndr.de/ndr/njoy/live/mp3/128/stream.mp3", 10.0f, false },
  { "FluxFM", "http://streams.fluxfm.de/klubradio/mp3-128/audio/", 8.0f, false },
  { "Suns LIVE", "http://stream.sunshine-live.de/live/mp3-192", 8.0f, false },
  { "Suns House", "http://stream.sunshine-live.de/house/mp3-192", 8.0f, false },
  { "Suns Party", "http://stream.sunshine-live.de/party/mp3-192", 8.0f, false },
  { "Suns Lounge", "http://stream.sunshine-live.de/lounge/mp3-192", 8.0f, false },
  { "Radio 10", "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO538.mp3", 9.0f, false },
  { "NDR 2", "http://icecast.ndr.de/ndr/ndr2/hamburg/mp3/128/stream.mp3", 10.0f, false },
  { "NDR 1 NDS", "http://icecast.ndr.de/ndr/ndr1niedersachsen/hannover/mp3/128/stream.mp3", 10.0f, false },
  { "1LIVE", "http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3", 10.0f, false },
  { "VRT Pop", "http://icecast.vrtcdn.be/ra2ant-high.mp3", 8.0f, false },
  { "Danmarks", "http://live-icy.gss.dr.dk:8000/A/A08H.mp3", 8.0f, false },
  { "JamFM", "http://stream.jam.fm/jamfm-live/mp3-128", 8.0f, false },
  { "Sveriges", "http://sverigesradio.se/topsy/direkt/701-hi-mp3", 8.0f, false },
  { "Energy SUI", "http://energyzuerich.ice.infomaniak.ch/energyzuerich-high.mp3", 8.0f, false },
  { "BFBS Ger", "http://tx.sharp-stream.com/icecast.php?i=ssvcbfbs5.aac", 8.0f, true } // no title info
};