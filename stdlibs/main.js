const MAX = 9000000;
const MAX_SQRT = 3000;
let FLAGS = [];

for(let i=0; i<MAX; i++) {
    FLAGS.push(true);
}

for(let i=2; i<MAX_SQRT; i++) {
    if(FLAGS[i]) {
        for(let k=i*2; k<MAX; k += i) {
            FLAGS[k] = false;
        }
    }
}

let msg = "";

for(let i=2; i<MAX; i++) {
    if(FLAGS[i]) {
        msg += String(i);
        msg += ", ";
    }
}

console.log(msg);

