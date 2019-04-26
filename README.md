# Audio-Streaming-Protocol 
🎶

Built on top of TCP protocol, the source code includes an user - client audio protocol for streaming music.
It features 5 quality levels, that are updated reviewing the checksum computation RFC-1071.
Depending on the quality, the audio is compressed with less or bigger intensity using both Downsampling and bit reduction techniques.
