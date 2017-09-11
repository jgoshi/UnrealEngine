// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineSubsystemTypes.h"
#include "JsonSerializerMacros.h"
#include "OnlineSubsystemTwitchPackage.h"

struct FTwitchTokenValidationResponse;

/**
 * Info associated with an user account generated by this online service
 */
class ONLINESUBSYSTEMTWITCH_API FUserOnlineAccountTwitch :
	public FUserOnlineAccount
{
public:

	// FOnlineUser
	
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override;

	// FUserOnlineAccount

	virtual FString GetAccessToken() const override;
	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	// FUserOnlineAccountTwitch
	FUserOnlineAccountTwitch(const FString& InUserId = FString(), const FString& InAuthTicket = FString())
		: UserId(MakeShared<FUniqueNetIdString>(InUserId))
		, AuthTicket(InAuthTicket)
	{
	}

	virtual ~FUserOnlineAccountTwitch() = default;

	/**
	 * Parse Json profile request data into the user account
	 *
	 * @param InAuthTicket previously associated auth ticket with this response
	 * @param ValidationResponse validation response from Twitch
	 * @return whether the parsing was successful or not
	 */
	bool Parse(const FString& InAuthTicket, FTwitchTokenValidationResponse&& ValidationResponse);

	/** @return Array of scope permissions we have to this account */
	const TArray<FString>& GetScopePermissions() const { return ScopePermissions; }

protected:

	/**
	 * Get account data attribute
	 *
	 * @param Key account data entry key
	 * @param OutVal [out] value that was found
	 *
	 * @return true if entry was found
	 */
	inline bool GetAccountData(const FString& Key, FString& OutVal) const
	{
		const FString* FoundVal = AccountData.Find(Key);
		if (FoundVal != nullptr)
		{
			OutVal = *FoundVal;
			return true;
		}
		return false;
	}

	/**
	 * Set account data attribute
	 *
	 * @param Key account data entry key
	 * @param Val value that we want to set
	 *
	 * @return true if entry changed
	 */
	inline bool SetAccountData(const FString& Key, const FString& Val)
	{
		const FString* FoundVal = AccountData.Find(Key);
		if (FoundVal == nullptr || *FoundVal != Val)
		{
			AccountData.Add(Key, Val);
			return true;
		}
		return false;
	}

	/** Any addition account data associated with the user */
	FJsonSerializableKeyValueMap AccountData;

	/** User Id represented as a FUniqueNetId */
	TSharedRef<const FUniqueNetId> UserId;
	/** Ticket which is provided to user once authenticated by the online service */
	FString AuthTicket;
	/** Scope permissions the user has authorized us for */
	TArray<FString> ScopePermissions;
};

/** Mapping from user id to account info and associated auth */
typedef TMap<FString, TSharedRef<FUserOnlineAccountTwitch> > FUserOnlineAccountTwitchMap;
