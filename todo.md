## Thoughts
Change expiration time to be something in the db, rather than client side

Create another table to hold the amount of hardware registrations

Logout user after 30 days of inactivity, or logout after 30 days regardless of activity?

Logout button, that will ask user to sign in again but not ask for key if registered

## App Todo

[//]: # (1. Create a button that will be authroize/status)

[//]: # (2. Remove automatic pop up of status on boot)

[//]: # (3. Add a logout button )

[//]: # (   1. The logout should redirect the user to the login)

[//]: # (   2. logout will set an internal instance to stop the plugin from working)

[//]: # (   3. Login will re-enable the plugin, as long as we get a 200 response and the hardware key matches)

[//]: # (   4. If for some reason it doesn't, go to authorize. )

[//]: # (4. Add periodic check of login)

[//]: # (   1. Save this locally, either on a keychain, or apperently juce::blowfish, whatever that is)

I now have an app that updates with the new table and has a logout. I need to include the webhook in pocketbase to 
limit the amount of activations of the license. If they logout, that will delete the license and stop processing. 
That will also allow it so if they close out of the session it won't just log them back in. Once I complete this,
I believe I've handled all the scenarios I need to and can move on to go. I may want to figure out how to set up gumroad,
but I think until I have a new plugin in which I want to do tis with, I think just keeping it local will be fine. I also
need to change how I handle the time stamps before I send a reauthentication request.

1. Look into sending one patch instead of one post and one patch [DONE]
2. Logout deletes from the 'activations' table -> Changed to encryption so file sharing is not possible, stay activated in table
3. Change time stamp handling [DONE]
4. Set up webhook? [DONE]
   

## DB Todo
1. Update tables to be able to have multiple hardware keys. [DONE]
2. Use this new table to set activation time -> handled on the client, just keep track of last seen
   1. Once the plugin is registered, set the time stamp
   2. on subsequent authority checks also verify the time stamp is less than 30 days dif then current time
   3. If false, logout user and reprompt sign in
   4. with successful sign in and verification, reset timestamp

## Prompt

Please add a button the gui. Put it inbetween the power button and the center text.
this button will change text depending on the current state of the user sign in. It will display "Sign In" if the user is not
signed in, "Auth" If the user is signed in but not authorized, and "Status" if the user is authorized.
In both the authorize and the Status window, add a button to logout of the plugin. This will change the button back to 
the login function, as well as allowing the user to log in. Add a variable to keep track of these stats in the plugin processor
"Authorized" for being authorized, "unauthorized" if they are logged in but not authorized, and "loggedOut" if they are not 
signed in. If the status is not authorized, then have the process function in the plugin processor return before processing any data.
If the user is not signed in, continue with the pop-up on start up, however, if the user is signed in, do not display the status pop up,
just boot the app as normal.

I have changed the db structure in relation to hardware ID's; the parsing logic in login will need to change. See attached json
example at the end for implementation. Instead of having the hardware key be a field in the plugin table, it is now in the expand of deviceIDs.
this table will store the hardware key, a "lastSeen" field, which will be the current time, as well as the id from the registeredPlugins. 
The id will go into an array called 'plugin' in the api. When verifying the hardware ID, loop through this array that is given back by the 
server. If there are no matches send the user to the auth window. Remove the check for registered elsewhere in the login function.
Once the user sends in their auth key, The server will verify if the user is allowed to create a new entry. If they cannot, 
you will receive a 400 response and a message. Display the message to the user in the auth pop up. If there is a success message
from the server, continue on as normal.

I would like to encrypt the session token that is received from pocketbase so that users session can stay persistent across
sessions. Instead of doing a license file, I just want to store the encrypted session ID, as well as time stamp. More info later 
When encrypting the token, we will salt  the response with the already implemented get hardware fingerprint method. 
With a valid login and plugin found, store the salted token received from pocketbase. Also do this in the authorize method on
successful authorization. When the user logs out, delete this session token. In the start auth flow method, instead of what we 
currently have for valid licenses, I want to decrypt the session token and send an auth refresh request for the user collection. 
If the status is 200, we can verify that the user is authorized, we can grab the new token and update our file. If the user is 
offline, as in, after the timeout time is expired and a status  code of zero is received, create and store a last authorized 
date/time stamp. I want a check that if this date is greater than 14 days they will have to reconnect to the internet. 
Reset this time everytime the user successfully authorizes again.
