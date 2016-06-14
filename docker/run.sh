#!/bin/bash
if [ -z "$KEYS" ]
then 
KEYS="/keys/broker-keys.json"
else
echo "keys from $KEYS"
fi

ARGS="-r $BROKER_PORT -s $BROKER_SCOPE -k $KEYS" 

if [ -z "$DEALER_PORT" ] 
then 
echo "No dealer, will be root"
else 
  echo "Dealer at $DEALER_PORT" 
  ARGS="$ARGS -d $DEALER_PORT"
fi 

if [ -z "$DEBUG" ] 
then
echo "Default debuglevel" 
else
  ARGS="$ARGS -l $DEBUG"
  echo "setting debuglevel $DEBUG"
fi   

if [ -z "$REST" ] 
then
echo "No REST Interface" 
else
echo "Starting REST interface at $REST"
  ARGS="$ARGS -w $REST"
fi   

ddbroker $ARGS 

