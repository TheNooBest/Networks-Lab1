#pragma once

enum send_type : uint32_t {
	// Types
	INTEGER,
	STRING,

	// Commands
	LOGIN,
	LOGOUT,
	GET_USERS,
	GET_MESSAGE,
};