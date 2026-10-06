#import <Foundation/Foundation.h>

// Darling has no smart card or token service: the watcher reports no tokens and its handlers never run.
@interface TKTokenWatcher : NSObject
@end

@implementation TKTokenWatcher

- (instancetype)initWithInsertionHandler:(void (^)(NSString *tokenID))insertionHandler
{
	return [self init];
}

- (NSArray *)tokenIDs
{
	return @[];
}

- (void)setInsertionHandler:(void (^)(NSString *tokenID))insertionHandler
{
}

- (void)addRemovalHandler:(void (^)(NSString *tokenID))removalHandler forTokenID:(NSString *)tokenID
{
}

- (id)tokenInfoForTokenID:(NSString *)tokenID
{
	return nil;
}

@end
