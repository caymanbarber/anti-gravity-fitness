import { initializeApp } from 'firebase/app';
import { getDatabase, ref, onValue, remove, update, hasChildren, set, get, orderByKey, parent, child, on } from 'firebase/database';
import { getFirestore, collection } from 'firebase/firestore';
import {inv, matrix, subset, ones, range, transpose, get as matrixget, index, multiply} from 'mathjs';

// For Firebase JS SDK v7.20.0 and later, measurementId is optional

const firebaseConfig = {

    apiKey: "AIzaSyAnF6uK1TDAmdqoieIb-ySmJJE28gNz_Qs",
  
    authDomain: "anti-gravity-fitness.firebaseapp.com",
  
    databaseURL: "https://anti-gravity-fitness-default-rtdb.firebaseio.com",
  
    projectId: "anti-gravity-fitness",
  
    storageBucket: "anti-gravity-fitness.appspot.com",
  
    messagingSenderId: "171793669016",
  
    appId: "1:171793669016:web:b4ce447aa3047a2e6e29ed",
  
    measurementId: "G-EXYF5HBBE1"
  
  };


var app = initializeApp(firebaseConfig);

var rtdb = getDatabase();


//var colRef = collection(db, 'books');

var bpm_ref = ref(rtdb, "bpm-data/");
var rpi_ref = ref(rtdb, "rpi-data/");
var rtdb_ref = ref(rtdb);
var workouts_ref = ref(rtdb, "Workouts/");




function get_heart_rate() {
    return heart_rate.heart_rate_now;
}


function trim_data(this_ref, snapshot, max_length) {
    
    const data = snapshot.val();
    const ks = Object.keys(data);
 
    const del_keys = ks.slice(0,ks.length - max_length);
    const keep_keys = ks.slice(ks.length - max_length +1 , ks.length - 1);

    const trimmed_data = {};
    for (let k in ks) {
        if (keep_keys.indexOf(ks[k]) > 0) {
            trimmed_data[ks[k]]=data[ks[k]];
        }
        
    }
    try {
        //this_ref.orderByKey.remove()
        
    } catch (e) {
        console.log(e);
    }

    try {
        for(let k in del_keys) {
            let del_ref = child(this_ref,del_keys[k]);
            //console.log(del_keys[k]);
            remove(del_ref);
        }
    } catch (e) {
        console.log(e);
    }

    //console.log("Trimmed");
    //console.log(JSON.stringify(trimmed_data));
    return trimmed_data;
}


var heart_rate= {
    heart_rate_now: 0,
    last_heart_rate: 0,
    max_heart_rate: 0
}


function delete_bpm_data() {
    let heart_rate_data = ref(rtdb, "bpm-data/");
    remove(heart_rate_data);
}
//delete_bpm_data();


onValue(bpm_ref, (snapshot) => {
    //console.log("value change")
    try {
        const bpm_data = trim_data(bpm_ref, snapshot, 10);
        if ( snapshot.hasChildren()) {
            
            let newest_log = bpm_data[Object.keys(bpm_data)[Object.keys(bpm_data).length - 1]];
            let newest_val = newest_log["bpm"];
            
            heart_rate.last_heart_rate = heart_rate.heart_rate_now;
            heart_rate.heart_rate_now = newest_val;

            for (let val in Object.values(bpm_data)){
                //console.log(Object.values(bpm_data)[val]);
                if (Object.values(bpm_data)[val]["bpm"] > heart_rate.max_heart_rate) {
                    heart_rate.max_heart_rate = Object.values(bpm_data)[val]["bpm"];
                }
            }
            const heart_rate_doc = document.getElementById("heartrate");
            heart_rate_doc.textContent = "Heartrate: "+ newest_val;

            const max_heart_rate_doc = document.getElementById("max-heartrate");
            max_heart_rate_doc.textContent = "Max Heartrate: "+ heart_rate.max_heart_rate;
            //console.log(data[0]);
        } else {
            
            throw "dates has no data";
        } 
    //} catch(NetworkError) {
    //    var rtdb = getDatabase();
    } catch (e) {
      console.log(e)
    }
});


var last_time = 0;
const delay_time = 5000;

onValue(rpi_ref, (snapshot) => {
    //console.log("value change")
    try {
        let rpi_data;
        if (snapshot.hasChildren()) {
            rpi_data = trim_data(rpi_ref, snapshot, 40);
        } else {
            return;
        }
        
        //console.log(JSON.stringify(rpi_data));
        if ( snapshot.hasChildren() && !paused) {
            
            let newest_log = rpi_data[Object.keys(rpi_data)[Object.keys(rpi_data).length - 1]];
            if (!newest_log) {
                return;
            } 
            //console.log(JSON.stringify(newest_log));
            let newest_val = newest_log["vZ"];

            let vel_arr = [];
            let acc_arr = [];
            let time_arr = [];

            let count = 0;
            for (let i in rpi_data) {
                vel_arr[count] = rpi_data[i]["vZ"];
                acc_arr[count] = rpi_data[i]["pZ"];
                time_arr[count] = rpi_data[i]["time"];
                count += 1;
            }
            //console.log("set up arrays")
            if (started && inactivity_check(acc_arr)) {         //TODO started
                //console.log("inactive")
                if (Date.now() - last_time > delay_time) {
                    inactive_state = true; 
                    console.log("Inactive");
                }
                //inactive_state = false; 

            } else {
                last_time = Date.now();
                inactive_state = false; 
                inactivity_start_time = time_arr[time_arr.length-1];
                let top_time = check_top(vel_arr,acc_arr,time_arr);
                //console.log("top time: " + top_time);
                if (!(top_time == false)) {
                    trim_to_time(rpi_ref, snapshot, top_time);
                    //console.log("found top, trimming");
                } else {
                    //console.log("checking bottom");
                    let bottom_time = check_bottom(vel_arr,acc_arr,time_arr);
                    //console.log("bot time: " + bottom_time);
                    if (!(bottom_time == false)) {
                        //console.log("found bottom, trimming");
                        trim_to_time(rpi_ref, snapshot, bottom_time);
                    }
                }
            }
            
            

            

            const speed_doc = document.getElementById("speed");
            speed_doc.textContent = "Speed: "+ newest_val;
            
        } else {
            
            throw "dates has no data";
        } 
    //} catch(NetworkError) {
    //    var rtdb = getDatabase();
    } catch (e) {
      console.log(e)
    }
});


var top_count = 0;
var bottom_count = 0;
var inactivity_start_time = 0;
var inactivity_thresh = 5; 
var inactive_state = false;
var paused = false; 
var started = false;


function check_top(vel_arr, acc_arr, time_arr) {

    //console.log("vel: " + vel_arr.toString());
    //console.log("acc: " + acc_arr.toString());

    const vel_thresh = 0.3;
    const acc_thresh_up = 1.2;

    let new_vel = [];
    let new_acc = [];

    for (let i in acc_arr) {
        if (acc_arr[i]>acc_thresh_up) {
            new_vel = vel_arr.slice(i,vel_arr.length);
            for (let j in new_vel) {
                if (new_vel[j] > vel_thresh) {
                    new_acc = acc_arr.slice(j,acc_arr.length);
                    for (let k in new_acc) {
                        
                        if (new_acc[k] < -1*acc_thresh_up) {
                            
                            let p = Number(i)+Number(j)+Number(k)+3;
                            if (p>new_acc.length -4){
                                break;
                            }
                            console.log("Top found");
                            let end_time = time_arr[p];
                            //console.log(end_time);
                            top_count++;
                            return end_time; 
                        }
                    }
                }
            }
        }
    }
    return false;
    //if top return true
    //else return false
}


function check_bottom(vel_arr, acc_arr, time_arr) {
    const vel_thresh = -0.3;
    const acc_thresh_down = -1.2;

    let new_vel = [];
    let new_acc = [];

    for (let i in acc_arr) {
        if (acc_arr[i]<acc_thresh_down) {
            new_vel = vel_arr.slice(i,vel_arr.length);
            for (let j in new_vel) {
                if (new_vel[j] < vel_thresh) {
                    new_acc = acc_arr.slice(j,acc_arr.length);
                    for (let k in new_acc) {
                        
                        if (new_acc[k] > -1*acc_thresh_down) {
                            let p = Number(i)+Number(j)+Number(k)+3;
                            if (p>new_acc.length -4){
                                break;
                            }
                            console.log("Bottom found");
                            //console.log(Number(i));
                            //console.log(Number(j));
                            //console.log(Number(k));
                            
                           // console.log(p);
                            //console.log("length: "+ new_acc.length);
                            let end_time = time_arr[p];
                            //console.log(end_time);
                            bottom_count++;
                            return end_time; 
                        }
                    }
                }
            }
        }
    }
    return false;
    //if top return true
    //else return false
}


function trim_to_time(this_ref, snapshot, time) {
    //console.log("trimming .., time = "+ time);
    const data = snapshot.val();
    const ks = Object.keys(data);
    let time_index = 0;
    //console.log(JSON.stringify(data));
    for(let t in ks) {
        if (time == ks[t]) {
            time_index = t;
            break;
        }
    }

    let time_match;
    for (let date_time in data) {
        if (time == data[date_time["time"]]) {
            time_match = date_time;
        }
    }
 
    const del_keys = ks.slice(0,time_match);
    //console.log(del_keys.toString());
    try {
        for(let k in del_keys) {
            let del_ref = child(this_ref,del_keys[k]);
            //console.log(del_keys[k]);
            remove(del_ref);
        }
    } catch (e) {
        console.log(e);
    }

}


function rep_check() {
    if (top_count > 0 && bottom_count > 0) {
        top_count = 0;
        bottom_count = 0;
        console.log("Rep found");
        return true;
    } else {
        return false;
    }
}


function inactivity_check(acc_arr) {
    const acc_thresh = 0.5;
    //console.log(acc_arr.toString());
    for(let i in acc_arr) {
        //console.log(acc_arr[i]);
        if(acc_arr[i]>acc_thresh || acc_arr[i]<-1*acc_thresh) {
            //console.log("activity");
            return false;
        }
    }
    return true
    //return false if not inactive
}

var new_workout_name = "";
async function get_workouts() { //use snapshots 
    //console.log("in get workouts");
    
    var workouts = {};
    get(child(rtdb_ref,"Workouts/")).then((snapshot) => {
        //console.log("in snapshot")
        workouts = snapshot.val()

    }), reason => {console.log(reason)};


    await sleep(1000);
    //console.log("not error");
   
    //console.log("JSON.stringify(workouts)");
    console.log(JSON.stringify(Object.keys(workouts)));
    let last_workout_name = Object.keys(workouts)[Object.keys(workouts).length - 1];
    const last_workout = workouts[last_workout_name];
    console.log(last_workout_name);
    const workout_num = last_workout_name.replace("workout","");
    new_workout_name = "workout" + (Number(workout_num) + Number(1));
    
    const ref_reps = 1;
    var workout_data = {
        "workouts": workouts,
        "new_workout": null,
        "new_workout_name": new_workout_name, 
    };
    return workout_data;
}


function get_workout_data(workout) {
    const avg_max_bpm = (workout["set1"]["max-bpm"]+workout["set2"]["max-bpm"])/2;
    const avg_reps = (workout["set1"]["reps"]+workout["set2"]["reps"])/2;
    const workout_data = {
        "ref-reps":workout["ref-reps"],
        "avg-max-bpm":avg_max_bpm,
        "avg-reps": avg_reps,
        "weight":workout["weight"]
    };
    return workout_data;
}


function save_workout(workout_name, workout) {
    const workout_ref =  ref(rtdb, "Workouts/"+workout_name+"/");
    set(workout_ref, workout);
    console.log("finished");
}


function update_reps(reps) {
    const rep_doc = document.getElementById("rep-num");
    rep_doc.textContent = "REPS: " + reps + "/12";
}


function update_command(command) {
    const command_doc = document.getElementById("command");
    command_doc.textContent = command;
}


function sleep(milliseconds) {  
    return new Promise(resolve => setTimeout(resolve, milliseconds));  
}  



/*
var sample_workout = {
    "ref-reps": 12,
    "set1": {
        "max-bpm": 100,
        "reps": 10
    },
    "set2": {
        "max-bpm": 100,
        "reps": 10
    },
    "weight":10,
    "workout-name": "overhead-press"
}
*/


async function calculate_weight(){  //using projection theorem find the calculated next reps
    //est_weight = (avg-reps)*c0 + (avg-max-bpm)*c1 + (n)*c2 + (1)*c3 
    //const T = [T[0],T[1],...,T[n]]
    try {
        let T = [];
        var act_weight = [];

        console.log("Getting workouts");
        const workout_list = await get_workouts();
        console.log("Workout gotten");
        const new_name = workout_list["new_workout_name"];
        const workouts = workout_list["workouts"];

        let ind = 0;

        for (let workout in workouts) {
            let new_workout = get_workout_data(workouts[workout]);
            act_weight[ind] = [new_workout["weight"]];
            T[ind] = [new_workout["avg-max-bpm"],new_workout["avg-reps"],ind];
            ind+=1;
        }
////
////
///
        T = matrix(T);
        const Plen = T.size()[1]; //4
        const datalen = T.size()[0]; //3
        let p0 = subset(T,index(range(0,datalen),0));
        let p1 = subset(T,index(range(0,datalen),1));
        let p2 = subset(T,index(range(0,datalen),2));
        let p = [p0, p1, p2];

        //let W = matrix([5,10]);

        console.log("going into for loop");

        let R = ones(Plen,Plen);

        for (let i = 0; i < Plen; i++) {
            for (let j = 0; j < Plen; j++) {
                //console.log("i: " +i+ " j: "+j)
                //console.log(p[i].toString());
                //console.log(transpose(p[j]).toString());
                let l = multiply(
                    transpose(p[j]),p[i]
                );
                //console.log(l.toString());
                //console.log(l.valueOf()[0][0]);
                R.subset(index(i,j),l.valueOf()[0][0]);
            }
        }
        //console.log(R.toString());
        
        let r = ones(Plen);
        let act = matrix(act_weight);

        for (let i = 0; i < Plen; i++) {
            let h = multiply(transpose(act),p[i]);
            //console.log(h.toString());
            //console.log("h: "+ h.valueOf()[0]);
            r.subset(index(i),h.valueOf()[0][0]);
        }
        console.log("r: "+r.toString());

        let c = multiply(inv(R),r);

        console.log("c: " + c.toString());

        
        let t = matrix([120,12,datalen]);

        console.log("t: " + t.toString());

        let new_weight = multiply(c,transpose(t)).valueOf();
        console.log(multiply(c,transpose(t)).toString());
        console.log("new weight: " + Math.round(new_weight));

        return Math.round(new_weight);
    } catch (e) {
        console.log(e);
    }
}


//var weight_start = false;
async function start_workout() {
    console.log("Calculating weights");
    const weight = await calculate_weight();
    update_command("Change wait to " + weight+ " lbs.");

    await sleep(5000); 
    
    console.log("Calculated weights");
    //find data for each workout
        //get past workouts
        //const workout_name_num          //TODO
        //find line of best fit 
            // avg-reps + avg-max-bpm + weight => weight to achieve reps=12 and avg-max-heartrate of last workout
            //get weight 

    //await sleep(10000);
    await sleep(1000); 

    update_command("Starting ...");
    connect_dumbbell();
    await sleep(2000); 
    reset_dumbell();

    await sleep(250);
    subscribe_dumbbell();

    await sleep(250);
    reset_dumbell();

    await sleep(1000);
    update_command("3");
    await sleep(1000);  
    update_command("2");
    await sleep(1000);  
    update_command("1");
    await sleep(1000);  
    update_command("Start!");
    reset_dumbell();

    await sleep(1000);

    // first set
    var reps = 0;
    started = true;
    last_time = Date.now();
    while (running) {
        //console.log(bottom_count);
        //console.log(top_count);
        if(rep_check()) {
            console.log("rep counted");
            reps ++;
            update_reps(reps);
            
        }
        await sleep(100); 
        if(inactive_state == true) {
            console.log("inactive");
            break;
        }
        //if dumbells placed down break
    }
    if (!running) {
        return
    }
    update_command("Great Job, now rest");
    //stop_polling();


    //save set1 maxhr, rep count
    const set1 = {
        "max-bpm": heart_rate.max_heart_rate,
        "reps": reps 
    }
    await sleep(3000); 
    


    

    //rest

    for (let countdown = 15; countdown > 0; countdown--) {
        update_command(countdown);
        
        await sleep(1000); 
    } 
    update_command("Pick up dumbell");
    reset_dumbell();
    await sleep(3000); 
    update_command("Start!");
    heart_rate.max_heart_rate = 0;





    reps = 0;
    started = true;
    last_time = Date.now();
    inactive_state = false;

    while (running) {
        //console.log(bottom_count);
        //console.log(top_count);
        if(rep_check()) {
            console.log("rep counted");
            reps ++;
            update_reps(reps);
            
        }
        await sleep(100); 
        if(inactive_state == true) {
            console.log("inactive");
            break;
        }
        //if dumbells placed down break
    }
    if (!running) {
        return
    }
    update_command("Great Job, all done!");
    stop_polling();

    const set2 = {
        "max-bpm": heart_rate.max_heart_rate,          //TODO
        "reps": reps          //TODO
    }

    //add workout with new data
    const finished_workout = {
        "ref-reps": 12,
        "set1": set1,
        "set2": set2,
        "weight":weight,           //TODO
        "workout-name": "overhead-press"
    }
    save_workout(new_workout_name, finished_workout);
    await sleep(3000); 
    
    










}

/*
function find ref reps

run before each workout to find number of reps needed 

inputs 
    workouts -> target rep, avg completed rep, and max heartrate 
    linear regression -> try to fit logistic function with projection theorem 

outputs
    projected rep number

*/


/*
getDocs(colRef)
  .then((snapshot) => {
      console.log(snapshot.docs)
  });
*/
function pause_workout() {}



function get_target_reps() {

}



function connect_dumbbell() {
    //firebase connect
    console.log("Connecting");
    const connect_key = "connect";
    const connect_value = 1;
    const config_reference = ref(rtdb, "Config/" + connect_key);
    set(config_reference, connect_value);
    connected = true;
}

var connected = false;
function disconnect_dumbbell() {
    //firebase disconnect
    console.log("Disconnecting");
    const disconnect_key = "disconnect";
    const disconnect_value = 1;
    const config_reference = ref(rtdb, "Config/" + disconnect_key);
    set(config_reference, disconnect_value);
    connected = false;
}

function subscribe_dumbbell() {
    //firebase subscribe
    console.log("Subscribing");
    const subscribe_key = "subscribe";
    const subscribe_value = 1;
    const config_reference = ref(rtdb, "Config/" + subscribe_key);
    set(config_reference, subscribe_value);
}

function start_polling() {
    //firebase on
    console.log("Starting polling");
    const start_key = "command";
    const start_value = 1;
    const config_reference = ref(rtdb, "Config/" + start_key);
    set(config_reference, start_value);
}

function stop_polling() {
    //firebase off
    console.log("Stopping polling");
    const stop_key = "command";
    const stop_value = 2;
    const config_reference = ref(rtdb, "Config/" + stop_key);
    set(config_reference, stop_value);
}

function reset_dumbell() {
    //firebase reset
    console.log("Resetting dumbell");
    const reset_key = "command";
    const reset_value = 3;
    const config_reference = ref(rtdb, "Config/" + reset_key);
    set(config_reference, reset_value);
}

var connection_init = false;

function connection_dumbbell() {
    var button = document.getElementById("connection-dumbbell");
    if (connection_init) {
        disconnect_dumbbell();
        button.textContent = "Connect To Dumbbell";
        connection_init = false;
    } else {
        connect_dumbbell();
        subscribe_dumbbell();
        button.textContent = "Disconnect To Dumbbell";
        connection_init = true;
    }
    //firebase connect\
}

var running = false;
function run_start() {
    if (running) {
        console.log("ending");
        start_btn.textContent = "Start Workout";
        try{
            stop_polling();
        } catch (e) {
            console.log(e);
        }
        
        running = false;
    } else {
        console.log("starting");
        start_workout();
        start_btn.textContent = "End Workout";
        running = true;
    }
    //firebase connect\
}

function test() {
    console.log("starting 1");
}

var reset_btn = document.getElementById("reset-dumbbell");
reset_btn.addEventListener('click', reset_dumbell);


var connection_btn = document.getElementById("connection-dumbbell");
connection_btn.addEventListener('click', connection_dumbbell);

var start_btn = document.getElementById("start-workout");
start_btn.addEventListener('click', run_start);

var start_btn = document.getElementById("start-workout");
start_btn.addEventListener('click', run_start);

function resume() {
    resume_btn.style.visibility = "hidden";
    return true;
}

var resume_btn = document.getElementById("resume-workout");
resume_btn.addEventListener('click', resume);
resume_btn.style.visibility = "hidden";