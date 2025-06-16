# Taco Quest Game Design

## Summary

Classic snake game with a multiplayer twist...and tacos!

- Each player controls a snake in an arena/level
- Snakes fight for tacos that spawn
- When a snake eats a taco, it grows longer

## Ideas

- Koopa shell-esque item to slow down other player
- Each player can bite the other player's tail and sever it
- Players can pass underneath their own tail when moving perpendicular to it
- How can we make the snake pooping a game mechanic ? Maybe a way for the snake to shrink ?
- Arrow keys allow you to control a second snake on one computer
- Snake gets item to speed up/slow down time for oneself or all players
- Easter egg for players who use port 6969
- Sometimes it feels like inputs are dropped when doing things like diagonal movements ?
  - Maybe queue up inputs within the same tick that could be executed in subsequent ticks
- Each player is a wyrm (mythical serpent), level is a medieval town, player destroys houses, etc... to gain segments instead of tacos
- Constriction: if there are one space gaps that can't be filled by continuing to constrict, add squeezing animation to fill those spaces to make the squeeze feel satisfying

## 1/26 Brainstorming Session

Competative coop
Severing tail -> turns back into apples which either opponent can eat
Colliding into a wall:
- no consequence (just stop)
- or, die, respawn at original len, body turns to apples (opportunity to recover len)
door requires two (or more?) buttons simult. to open
- short: impossible (without coop!) long: possible
- maybe area of level that has good stuff, only accessible to a long snake
Apples don't spawn randomly: they "grow", required one to make plans
Apples really are random:
- 1/3: get lucky
- 1/3: unlucky (spawns near the other player)
- 1/3: both are same distance: race
Apples spawn both randomly and with 'growing' version
Level wraps around?
power-up that enables tail snipping (otherwise normal snake mechanic: die when hitting other)
Penalty if you get snipped
Penalty for the snipper: momentary freeze
low amount of food spawns: more in frequent
speed power, but become snippable


4/13/25
Playtest Notes:
- When a snake gets stuck, the game is basically over
- Tacos allow the snake to get longer, but there is no longer term goal
- Something fun about cutting off and taking space
- 

Ideas:
The game needs a goal: Kill the other snake, but how ?

Snake has the ability (must be activated) to bite through a snake body, whether
is their own or the other snake. Snake segments towards tail turn into tacos
- Allowing to always do this might lead to chaos, how can we limit ?
  - a 15-20 second cooldown.
  - requires eating a certain number of tacos
  - hybrid of the first 2
- Used to get snakes unstuck, at the cost of time and snake length
- Used to attack opponent
- Does being in chomp mode mean you are vulnerable in some way, like the other
  snake can eat you too ?
- level structure and pick-ups that do... ?

Variety of items with different values.

Try with more players, and maybe more walls to add some map variety

What if chomping the other snake's head kills the snake ?
- How does growing longer help us here ?
  - Easier to use your body to block when you don't have the ability to chomp ?
- What if you have a choice, when you eat a taco, to have it grow your length
  or allow you to chomp.
- What if you can use some length to allow yourself to chomp ?
- What if you can poop ? Setting up land-mines for the enemy
