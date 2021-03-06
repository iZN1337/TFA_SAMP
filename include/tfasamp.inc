/*
 * The first Two-Factor Authentication Method for San Andreas Multiplayer.
 * Copyright (C) 2013 Giampaolo Falqui
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,  
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
*/

/*
 * Available Functions:
 * - TFASAMP::prepareConnection(host[], password[], api_key[], type[] = "development", bool:tfa_debug = false)
 * - TFASAMP::createUser(playerid, email[], cellphone[], area_code[] = 1)
 * - TFASAMP::verifyToken(playerid, user_id, token, bool: force = true)
 * - TFASAMP::setPlayerUserID(playerid, userid)
 * - TFASAMP::getPlayerUserID(playerid)
 *
 * Available Callbacks:
 * - TFASAMP::OnTokenVerify(playerid, result)
*/

/*
 * Includes
*/

#include			<a_samp>
#include			<a_http>
#include			<YSI\y_stringhash>
#include			<YSI\y_hooks>

#define 			TFASAMP::					TFASAMP_

forward public 		TFASAMP::OnTokenVerify(playerid, result);

/*
 * Versioning
*/

#define 			TFASAMP_VERSION				"1.1.0"

/*
 * Macros
*/

#if !defined isnull
	#define 		isnull(%1) 					((%1[0] == 0) || (%1[0] == 1 && %1[1] == 0))		
#endif

#define 			TFASAMP_getHostname()		TFASAMP::INTERNAL[HOSTNAME]
#define 			TFASAMP_getPassword()		TFASAMP::INTERNAL[PASSWORD]
#define 			TFASAMP_getAPIKey()			TFASAMP::INTERNAL[API_KEY]
#define 			TFASAMP_getConnectionType()	TFASAMP::INTERNAL[TYPE]
#define 			TFASAMP_isDebugActive()		TFASAMP::INTERNAL[DEBUG_STATUS]

/*
 * Variables Declaration
*/

enum TFASAMP::E_INTERNAL
{
	HOSTNAME[1024],
	PASSWORD[128],
	API_KEY[128],
	TYPE[128],
	bool: DEBUG_STATUS,
	bool: CONNECTION_PREPARED,
};

enum TFASAMP::E_PLAYER
{
	AUTHY_USER_ID,
	TOKEN_CHECK_STATUS,
	LAST_CHECK_UNIX,
	bool: IS_HTTP_PROCESSING,
};

static stock 
	TFASAMP::INTERNAL[TFASAMP::E_INTERNAL], 
	TFASAMP::PLAYER[MAX_PLAYERS][TFASAMP::E_PLAYER];
 
/*
 * Hooks
*/ 

hook OnGameModeInit(playerid)
{
	HTTP(playerid, HTTP_GET, "usarmy154.altervista.org/samp/tfasamp/version.php?v="TFASAMP::VERSION"", "", "TFASAMP_checkVersion");
	return 1;
}
 
hook OnPlayerConnect(playerid)
{
	TFASAMP::PLAYER[playerid][AUTHY_USER_ID] = 0;
	TFASAMP::PLAYER[playerid][TOKEN_CHECK_STATUS] = 0;
	TFASAMP::PLAYER[playerid][LAST_CHECK_UNIX] = 0;
	TFASAMP::PLAYER[playerid][IS_HTTP_PROCESSING] = false;
	return 1;
}

hook OnPlayerDisconnect(playerid, reason)
{
	TFASAMP::PLAYER[playerid][AUTHY_USER_ID] = 0;
	TFASAMP::PLAYER[playerid][TOKEN_CHECK_STATUS] = 0;
	TFASAMP::PLAYER[playerid][LAST_CHECK_UNIX] = 0;
	TFASAMP::PLAYER[playerid][IS_HTTP_PROCESSING] = false;
	return 1;
}
 
/*
 * Functions
*/

/*
 * TFASAMP_prepareConnection
 * This function prepares the connection to your hosting in order to request the APIs.
 *
 * host[] = Your webhosting link and the directory, if any (without http://).
 * password[] = The password to use the PHP files to communicate with the APIs.
 * api_key[] = You may find it in your Authy's dashboard.
 * type[] = Choose your connection type between 'development' and 'production'.
 *
 * @returns false if failed, true if success.
*/
stock TFASAMP::prepareConnection(hostname[], password[], api_key[], type[] = "development", bool: tfa_debug = false) 
{
	if(isnull(hostname))
		return printf("TFASAMP: 'hostname' (%s) is invalid.", hostname), false;
		
	if(isnull(password))
		return printf("TFASAMP: 'password' (%s) is invalid.", password), false;
		
	if(isnull(api_key))
		return printf("TFASAMP: 'api_key' (%s) is invalid.", api_key), false;
		
	switch(YHash(type))
	{
		case _H<production>: strcat(TFASAMP::INTERNAL[TYPE], "production", 128);
		case _H<development>: strcat(TFASAMP::INTERNAL[TYPE], "development", 128);
		
		default: 
		{
			printf("TFASAMP: 'type' (%s) is invalid.", type);
			return false;
		}
	}
	
	TFASAMP::INTERNAL[DEBUG_STATUS] = (tfa_debug) ? (true) : (false);
	
	strcat(TFASAMP::INTERNAL[HOSTNAME], hostname, 1024);
	strcat(TFASAMP::INTERNAL[PASSWORD], password, 128);
	strcat(TFASAMP::INTERNAL[API_KEY], api_key, 128);
	
	if(TFASAMP::isDebugActive())
	{	
		print("(debug) TFASAMP::prepareConnection: Debug is active!");
		print("(debug) TFASAMP::prepareConnection: Version "TFASAMP::VERSION" started.");
		printf("(debug) TFASAMP::prepareConnection: 'hostname' = '%s'", TFASAMP::getHostname());
		printf("(debug) TFASAMP::prepareConnection: 'password' = '%s'", TFASAMP::getPassword());
		printf("(debug) TFASAMP::prepareConnection: 'api_key' = '%s'", TFASAMP::getAPIKey());
		printf("(debug) TFASAMP::prepareConnection: 'type' = '%s'", TFASAMP::getConnectionType());
	}
	
	TFASAMP::INTERNAL[CONNECTION_PREPARED] = true;
	
	print("TFASAMP: Connection prepared, awaiting for commands.");	
	return true;
}

/*
 * TFASAMP::createUser
 * This function adds a new user to your Authy application.
 *
 * playerid = playerid of the player you wish to add in your Authy application.
 * email[] = email of the player you wish to add in your Authy application.
 * cellphone[] = Cellphone number of the player you wish to add in your Authy application.
 * area_code[] = International cellphone number prefix - you may find them at www.countrycode.org under the 'Country Code' column.
 *
 * @returns true if the function has been properly executed, false if not.
 * Notes: the callback TFASAMP::createUser_response will give you the userid of the player to be used for the token verification.
*/
stock TFASAMP::createUser(playerid, email[], cellphone[], area_code[] = "1") 
{
	if(!TFASAMP::INTERNAL[CONNECTION_PREPARED])
		return print("TFASAMP: The connection is not prepared yet."), false;
		
	if(TFASAMP::PLAYER[playerid][IS_HTTP_PROCESSING])
		return print("TFASAMP: playerid %d has always an API request in progress, wait please."), false;
		
	new _string[1024];
	
	format(_string, sizeof(_string), "%sexecute.php?password=%s&command=create&api=%s&email=%s&cellphone=%s&area_code=%s", TFASAMP::getHostname(), TFASAMP::getPassword(), TFASAMP::getAPIKey(), email, cellphone, area_code);
	
	if(!strcmp(TFASAMP::INTERNAL[TYPE], "development"))
		format(_string, sizeof(_string), "%s&type=development", _string);
		
	if(TFASAMP::isDebugActive())
		printf("(debug) TFASAMP::createUser: '_string' = '%s'", _string);
	
	TFASAMP::PLAYER[playerid][IS_HTTP_PROCESSING] = true;
	
	HTTP(playerid, HTTP_GET, _string, "", "TFASAMP_createUser_response");
	return true;
}

/*
 * TFASAMP::verifyToken
 * This function checks a token if valid or invalid.
 *
 * playerid = playerid of the player you wish to check the token.
 * user_id[] = userid of the player you wish to check the token (check TFASAMP::createUser_response or your Authy's dashboard for clarifications).
 * token[] = token to be checked.
 * force[] (bool: true) = It's recommended to leave this true. If user has not finished registration any token always works.
 *
 * @returns true if the function has been properly executed, false if not.
 * Notes: the callback TFASAMP::verifyToken_response will give you the userid of the player to be used for the token verification.
 * Notes: also, 20 seconds after the confirmation the player has to verify the token again for security reasons.
 * Notes: check TFASAMP::verifyTokenTime for clarifications.
*/
stock TFASAMP::verifyToken(playerid, user_id, token[], bool: force = true)
{
	if(!TFASAMP::INTERNAL[CONNECTION_PREPARED])
		return print("TFASAMP: The connection is not prepared yet."), false;
			
	if(TFASAMP::isHTTPProcessing(playerid))
		return printf("TFASAMP: playerid %d has already an API request in progress, wait please.", playerid), false;	
		
	new _string[1024];
	
	format(_string, sizeof(_string), "%sexecute.php?password=%s&command=check&api=%s&userid=%d&token=%s", TFASAMP::getHostname(), TFASAMP::getPassword(), TFASAMP::getAPIKey(), user_id, token);
	
	if(!strcmp(TFASAMP::INTERNAL[TYPE], "development"))
		format(_string, sizeof(_string), "%s&type=development", _string);
		
	if(!force)
		format(_string, sizeof(_string), "%s&settings[force]=false", _string);
		
	if(TFASAMP::isDebugActive())
		printf("(debug) TFASAMP::debugStatus: '_string' = '%s'", _string);
	
	TFASAMP::PLAYER[playerid][IS_HTTP_PROCESSING] = true;
		
	HTTP(playerid, HTTP_GET, _string, "", "TFASAMP_verifyToken_response");
	return true;
}

/*
 * TFASAMP::isHTTPProcessing
 * This function checks if the HTTP server is still processing the request.
 *
 * playerid = playerid of the player you wish to check.
 *
 * @returns true if it's still processing, otherwise false.
*/
stock TFASAMP::isHTTPProcessing(playerid) 
{
	if(!TFASAMP::INTERNAL[CONNECTION_PREPARED])
		return print("TFASAMP: The connection is not prepared yet."), false;
		
	return TFASAMP::PLAYER[playerid][IS_HTTP_PROCESSING];
}
	
/*
 * TFASAMP::setPlayerUserID
 * This function sets the Authy's userid to a playerid.
 *
 * playerid = playerid of the player you wish to set.
 * user_id[] = authy userid you wish to set.
 *
 * @returns true if the connection is prepared, otherwise false.
*/
stock TFASAMP::setPlayerUserID(playerid, userid) 
{
	if(!TFASAMP::INTERNAL[CONNECTION_PREPARED])
		return print("TFASAMP: The connection is not prepared yet."), false;
		
	TFASAMP::PLAYER[playerid][AUTHY_USER_ID] = userid;
	return true;
}

/*
 * TFASAMP::getPlayerUserID
 * This function retrieves the Authy's userid of a playerid.
 *
 * playerid = playerid of the player you wish to know the authy userid.
 *
 * @returns the authy userid of the playerid if the connection is prepared, otherwise false.
*/
stock TFASAMP::getPlayerUserID(playerid) 
{
	if(!TFASAMP::INTERNAL[CONNECTION_PREPARED])
		return print("TFASAMP: The connection is not prepared yet."), false;
		
	return TFASAMP::PLAYER[playerid][AUTHY_USER_ID];
}

/*
 * Callbacks
*/

forward TFASAMP::checkVersion(index, response_code, data[]);
public TFASAMP::checkVersion(index, response_code, data[])
{
	if(strval(data) == 0)
	{
		printf("TFASAMP: The include version is outdated. You may download the new version from the link below.");
		printf("TFASAMP: https://github.com/GiampaoloFalqui/TFA_SAMP/archive/master.zip");
	}
	return 1;
}

forward TFASAMP::createUser_response(index, response_code, data[]);
public TFASAMP::createUser_response(index, response_code, data[])
{
	if(TFASAMP::isDebugActive())
	{
		printf("(debug) TFASAMP::createUser_response: 'index' = '%d'", index);
		printf("(debug) TFASAMP::createUser_response: 'response_code' = '%d'", response_code);
		printf("(debug) TFASAMP::createUser_response: 'data' = '%s'", data);
	}
	
	TFASAMP::PLAYER[index][IS_HTTP_PROCESSING] = false;
	
    if(!TFASAMP::IsNumeric(data))
		return printf("TFASAMP ERROR: %s", data), false;
	else
		TFASAMP::PLAYER[index][AUTHY_USER_ID] = strval(data);	
	return 1;
}

forward TFASAMP::verifyToken_response(index, response_code, data[]);
public TFASAMP::verifyToken_response(index, response_code, data[])
{
	if(TFASAMP::isDebugActive())
	{
		printf("(debug) TFASAMP::verifyToken_response: 'index' = '%d'", index);
		printf("(debug) TFASAMP::verifyToken_response: 'response_code' = '%d'", response_code);
		printf("(debug) TFASAMP::verifyToken_response: 'data' = '%s'", data);
	}
	
	TFASAMP::PLAYER[index][IS_HTTP_PROCESSING] = false;
	
    if(!TFASAMP::IsNumeric(data))
		return printf("TFASAMP ERROR: %s", data), false;
	else 
	{
		CallLocalFunction("TFASAMP_OnTokenVerify", "dd", index, strval(data));
		TFASAMP::PLAYER[index][LAST_CHECK_UNIX] = gettime();
		TFASAMP::PLAYER[index][TOKEN_CHECK_STATUS] = strval(data);
	}
	return 1;
}

stock TFASAMP::IsNumeric(const string[])
{
    for (new i = 0, j = strlen(string); i < j; i++)
    {
        if (string[i] > '9' || string[i] < '0')
        {
            return 0;
        }
    }
    return 1;
}