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
"Conversation" is the only class that has raw 'new[]' and 'delete[]'. "Message" doesn't need special member functions because 'std::string' manages itself.
    - Destructor: 'delete[] data_'. This is a safe call that targets an object that has had
      its data taken by a move constructor/assignment
    - Copy Constructor/Assignment: Allocates a separate array and copy each element (deep      
      copies every string). Assignment builds the new buffer before freeing the old one and 
      guards against self-assignment
    - Move Constructor/Assigment: Takes 'data_', 'size_', 'capacity_', then leaves the source 
      empty but still reusable. Both are 'noexcept'.
The tests check each directly and confirm that every member function is present and works as intended.

## Sentinel scanner: bounded pending_ proof
If we let m = 'sentinel_size', then, after every call, 'pending_.size() =/< m-1'.
Proof: Initially, 'pending_' is empty. On each feed, the scanner forms 'buf = pending_ +chunk'. If 'buf' conatains the sentinel, then 'pending_' is cleared. Otherwise, it keeps a suffix of length = 'hold', which is chosen from a loop that starts at 'min(|buf|, m-1)' and only goes down. Therefore, 'hold =/< m=1' by construction.
No sentinel can be missed or leaked. If an occurence starts at position p of 'buf' but is incomplete, then 'buf[p:]' is a proper prefix of the sentinel and a suffix of 'buf', so its length is at most 'hold'. This means that p lies inside the held-back region. So, everything produced doesn't contain a sentinel start. A complete occurence is found by 'find'.
Space per call is O(m + |chunk|), not O(stream length). Re-Scanning the whole concatenation on every call would cost 1+2+...+N = O(N^2), when the stream arrives one byte at a time. In this project, total work is O(N*m^2) with m fixed at 20. 
The 4MB stress test ensures that the not-produced byte count stays =/< 19 and that the output plus 'flush()' is exactly equal to the input.

## What I would do differently
In the current iteration of my project, my copy constructor and copy assignment are not exception-safe. So, if copying a message throws partway through the loop, the newly allocated array is never freed and causes a leak. The tests don't cause this to occur, but it is a possibility. If I were doing this project over again, I would use the copy-and-swap idiom to build a temporary conversation, then swap it into '*this'. I would also remove the redundant cleanup logic in the assignment operators. 
