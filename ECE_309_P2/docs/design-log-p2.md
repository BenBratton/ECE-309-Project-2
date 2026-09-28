# Design Log — Project 2 - Benjamin Bratton
(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost
"Conversation" doubles it's size and capacity when its full. 
    - capacity_ == 0 ? 4 : capacity_ * 2
If we consder 'n' appends, a reallocation will only occur when 'size_==capacity_' (at sizes 4, 8, 16, etc) and each one moves 's' elements, where 's' is the size at that instance. The last reallocation occurs at size 's_last < n', so the total moved is:
    -> 4+8+...+s_last = s_last(1+1/2+1/4+...) < 2*s_last < 2*n 
Adding the 'n' direct writes and the total work is under 3*n, so append will cost, at most, 3 element operations amortized: O(1). I chose 2 over 1.5 because its the simplest factor to prove, it keeps allocation count logarithmic, and wastes, at most, half the buffer capacity. 
One of the tests confirms this. 1000 appends produces exactly 9 reallocations, with each doubling the previous capacity for 4*2^9-1=1024 while keeping each element intact after each reallocation.

## Rule of Five evidence
The rule of five states that if a class defines or deletes any one of the five special member functions responsible for resource management, it should explicitly define or delete all five.



## Sentinel scanner: bounded pending_ proof



## What I would do differently
