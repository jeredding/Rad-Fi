/*
Rad-Fi delay by Dr. Bleep
bleeplabs.com

*/


#include <TimerOne.h> // This library is necessary to creat the audio interupt

// the size of our delay buffer. the atmega328 only has 2k of ram. 
// We could probably use a little more than half but this is safe and easy to calculate

#define DLY_BUF_LEN 1030 
byte dly_buffer[DLY_BUF_LEN];

unsigned long d,t,prev,prev2;
byte tickLED,tick2,pot_tick;
unsigned int in0;
int osc_rate;
long dds_tune;
byte dly_tick,fast_pot_tick,slow_sample;
uint16_t write_head,read_head,lerp_dly,fm,am,samp_c,freq_in,freq_in_s;
int dly_filter,feedback,fb_tmp,fb_tmp_inv,output,dly_input,dly_time,rn;
int in2;
byte f_mode;
int seq_step;
long seq_rate,seq_step_len;
int seq_stop,p1,dly_buf_in,p2,p2_inv;
int dly_mix,dly_mix_2,fpot1,mode_p,seq_rate_p;
byte mode,seq_on,seq_amp_out;
 int amp_lerp,fb_amt,fb_amt_inv; 
byte crush_amt;
int  dly_led_c;
int blink_inv;
byte tick4;
int previnput[2]={};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  pinMode(19, OUTPUT); //delay rate LED
  pinMode(18, OUTPUT); //audio LED

  pinMode(2, INPUT);  // audio in
  digitalWrite(2,HIGH);



//delay rate select with binary value at 3 and 4
// both low = 0 = full speed 13.3 kHz
// 4 low 3 high(unattached) = 1 = 6.66 kHz  // recomended
// 4 high 3 low = 2 = 3.33 kHz  // it's pretty much garbage at thig point but it's fun for feedback noise! 
// both high = 3 = 1.66 kHz 

  pinMode(4, INPUT);   
  digitalWrite(4,HIGH);

  pinMode(3, INPUT);
  digitalWrite(3,HIGH);


  // PWM is a way of mimicking analog signals with digital pins. 
  // We need to change the frequecy so our output sounds better.
  //
  // http://playground.arduino.cc/Main/TimerPWMCheatsheet
  
  TCCR0B = TCCR0B & 0b11111000 | 0x01 ; //timer 0 62500Hz
  

  osc_rate=75; //75 microsconds = 13.3 kHz

  Timer1.initialize(osc_rate); 
  Timer1.attachInterrupt(Audio_interupt); 

  Serial.begin(9600);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Audio_interupt()
{
  tickLED=!tickLED;
  digitalWrite(13,tickLED);  //this allows us to attach a scope to pin 13 and see the actual rate the interut is going.


  dly_led_c++;
  if (dly_led_c>blink_inv){   
    tick4=!tick4;
    digitalWrite(19,tick4);   // blinks with the delay rate
    dly_led_c=0;
  }

  int v0;


// the input is actually a digial read but we're just producing square waves with the 40106
// and since analog read takes a little too long to do in thei quick look, digial works well. 
// in future versions we'll expore an analog input.

  byte in_3=!(digitalRead(2))*254;    // ths input is 0 or 1 but we need a bigger number
                                     // =! so it's inverted. Since we have the input pullup on it will rest high so this makes it low
                                     // 254 instaed of 255 since it seems to clip less before we want it to.

  byte t1 = feedback_delay(in_3);    // input the new value input delay and return t1 which is a mix of the input and output.

  analogWrite(5, t1);                // PWM out this signal on pin 5 


  if (t1>127){          
    digitalWrite(18,1);   // very siomple way to blink with audio. To be inproved
  }

  else        
    digitalWrite(18,0);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




byte feedback_delay(byte dly_input){

  // The delay works by writing and read values from a large bank of values.
  // Both "heads" are constatly moving through the bank value by value and wrapping around when the reach the top.
  // The length of the delay is determined by how far away the read head is from the write head.
//
//  For exampe lets say the wite head is at 100 and the length is 500. That means that the write head will take the input value and 
//  record it at the 100th position in the buffer, dly_buffer[100]. The read head would be at 100-500= -400 = 1030(the size of the bank)-400 = 630.
//  The output signal is added to the input signal with feedback. This is waht give the real delay sound. Other wise it would just be BEEP.....BEEP.
//  Add it back in gives the long trailing sound that diminishes in voulme. BEEPPPPP...BEEPPP....BEEP...Beep...be....b......
//  Adding a lot of feedback and you get...uuh.. feedback!
  
  
  
  dly_tick++;
  if (dly_tick>crush_amt){
    write_head++;        // here's where the binary input at 3 and 4 comes in. A high value means it only advances the heads
    dly_tick=0;

  }



  if (write_head > (DLY_BUF_LEN-2)){
    write_head=0; // wrap  it around 
  }

  read_head = (write_head + lerp_dly); // the read head is "behind" the write head by being lerp_dly in front of it in time

  if (read_head > (DLY_BUF_LEN-2)){
    read_head-=(DLY_BUF_LEN-2);   // wrap  it around 
  }

// feedback. As fb_tmp goes up, fb_tmp_inv goes down. once you get over half of the pot things getIUHUIH*U(*JNB%%^T*YyughyuvYGZCZXX
// we subtract 127 to combine the input and feedback values.
// Audio is push and pull from a central point but haing it be 0-255 (a byte) means that it rests at 127.
// when you add two numbers together like this you looks that middle. So we make the values -127 to 127, combine them, half it and add 127 to make it 0-255 again.


  int fb_in = ((feedback-127)*fb_tmp)>>7;   
  int in_in = ((dly_input-127)*fb_tmp_inv)>>9;
  dly_buffer[write_head] = (fb_in+in_in)+127;  // 


// here we read the output and calucle the levels of feedback, input, and ouput singals to be produced.
  byte dly_out = dly_buffer[read_head];
  dly_mix_2=(((dly_input-127)*70)>>8);
  dly_mix=((dly_out-127)*(255-70))>>8;

  int out_temp1 = dly_mix_2+dly_mix+127;

// So we get differnt types of feedback noise we make it so the values can't overflow untill we go over about half the pot

  if (fb_tmp<135){

    if (out_temp1>=253){
      out_temp1=253;
    }

    if (out_temp1<=2){
      out_temp1=2;

    }
  }

  feedback = out_temp1;
  return out_temp1;   // value to ouput. 

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// And finally the loop
// This cycles more slowly than the audio. 

void loop(void)
{

  p2=analogRead(A0);            
  dly_time=readchange(0,p2);    // update the delay time value. readchange info bleow

  fb_tmp=analogRead(A1)>>2;    // update the feedback value.
  fb_tmp_inv=(fb_tmp-255)*-1;  //calculates an inverted value for feedback amount


  // read the two inputs and caluclats the binary valuve for delay length. 
  byte bc3 =! digitalRead(3);  
  byte bc4 =! digitalRead(4);
  crush_amt=(bc3<<1)|bc4;


// though you can't rell too much with a short, lo-fi delay, the delay time needs to move around smoothly. 
// this code slowly changes delay time value so that it chases the actual pot value around rather than immediatley going to it and making noise we don't want.

  if ((millis()-prev2)>40){
    prev2=millis();
    byte dly_step=1;

    if(lerp_dly < dly_time-dly_step)  
      lerp_dly +=dly_step;
    if(lerp_dly > dly_time+dly_step)
      lerp_dly -= dly_step;
  }

  blink_inv=((dly_time-DLY_BUF_LEN)*-1);


}



/// this code is used to smooth out the pot values. it only changes the value if the pot moves more than "diff"
// if the pots have noisey reading this intoduces the wrong kind of clikcs and pops in our audio out. 

int readchange(byte n, int input){
  int diff=20;
  //int input = analogRead(n);
  int output;
  if ((input>(previnput[n]+diff))||(input<(previnput[n]-diff))){

    output= input;
    previnput[n]=input;
    //Serial.println("C");
  }


  else{
    output=  previnput[n];
    ///Serial.println("-");
  }

  if (input>1020){

    output=1024;
  }

  if (input<3){

    output=0;
  }

  return output;
}








