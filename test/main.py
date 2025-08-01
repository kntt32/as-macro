numbers = []

for i in range(2, 100000):
    prime_flag = True
    for k in range(0, len(numbers)) :
        if i % numbers[k] == 0 :
            prime_flag = False
            break

    if prime_flag :
        numbers.append(i)

print(numbers)

