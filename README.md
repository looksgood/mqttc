mqttc
=====

mqtt client written in c language.

usage
=====

mqttc -h host -p port -u username -P password -k keepalive

command
=======

publish topic qos message

subscribe topic qos

unsubscribe topic

compile
=====

cd src && make

redis
=====

mqttc is based on redis event library.

license
=======

BSD License
