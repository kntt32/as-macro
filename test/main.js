var numbers = [];

for(let i=2; i<100000; i++) {
    let prime_flag = true;
    for(let k=0; k<numbers.length; k++) {
        if(i % numbers[k] == 0) {
            prime_flag = false;
            break;
        }
    }
    if(prime_flag) {
        numbers.push(i);
    }
}

console.log(numbers);


