# hxcpp with Wii U support?

This fork of hxcpp (Based on commit ``1618253``) based on [my hxcpp fork for the Nintendo Switch](https://github.com/Slushi-Github/hxcpp-nx) has all the modifications needed to compile Haxe for the Nintendo Wii U.

**THIS IS TOTALLY EXPERIMENTAL AND I DON'T KNOW WHAT STATE THIS THING IS IN; I BARELY MANAGED TO PRINT SOMETHING TO THE CONSOLE SCREEN, I DO NOT RECOMMEND USING THIS, AND I DID IT PURELY FOR EXPERIMENTAL PURPOSES. DON’T EXPECT ME TO TAKE THIS SERIOUSLY, USE [hxcpp-nx](https://github.com/Slushi-Github/hxcpp-nx) INSTEAD, WHICH ACTUALLY WORKS!**

I'd really appreciate any help with this. 

## Problems:

The implementation of pthreads in DevKitPPC or specifically for the Wii U doesn't seem to be the best or sufficient enough to at least support TLS (**T**hread-**L**ocal **S**torage), so the Wii U couldn't use them correctly and caused hxcpp to crash. I've managed to implement a solution that allows a program compiled to use pthreads and TLS to run, but the program is still incredibly unstable, whether using a single thread or more.

This is how I feel after giving up on this project (VIDEO WITH LOUD SOUND, maybe?):

https://github.com/user-attachments/assets/79171259-06c3-43e1-9e41-523a89a67cfe
