/* Exercise 8-1

   printf("%ld %ld\n", (long) (getpwnam("avr")->pw_uid), (long) (getpwnam("tsr")->pw_uid));

   The above code calls getpwnam twice before printing.
   But getpwnam is not reentrant : it returns a pointer to a statically allocated buffer.
   That zone is erased after each call. So the two pointers refer to the same UID (avr's or tsr's, it is undefined!)
*/
