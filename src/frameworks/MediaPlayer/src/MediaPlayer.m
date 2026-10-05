/*
 This file is part of Darling.

 Copyright (C) 2026 Darling developers

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

// The part of MediaPlayer an application uses to publish what it is playing and to hear about media
// keys: MPNowPlayingInfoCenter, MPRemoteCommandCenter, MPMediaItemArtwork and their dictionary keys.
//
// What is published is kept and can be read back. Nothing shows it: Darling has no "Now Playing"
// interface. No remote command is ever delivered either, because nothing routes media keys here;
// targets are recorded so that adding and removing them behaves normally.
// ponytail: to make this live, bridge the centre to MPRIS on the session bus (publish the info, call
// the recorded targets on Play/Pause/Next/Previous).

#import <Foundation/Foundation.h>
#import <CoreGraphics/CGGeometry.h>

NSString *const MPMediaItemPropertyTitle = @"title";
NSString *const MPMediaItemPropertyArtist = @"artist";
NSString *const MPMediaItemPropertyAlbumTitle = @"albumTitle";
NSString *const MPMediaItemPropertyPlaybackDuration = @"playbackDuration";
NSString *const MPMediaItemPropertyArtwork = @"artwork";
NSString *const MPNowPlayingInfoPropertyElapsedPlaybackTime = @"MPNowPlayingInfoPropertyElapsedPlaybackTime";
NSString *const MPNowPlayingInfoPropertyPlaybackRate = @"MPNowPlayingInfoPropertyPlaybackRate";
NSString *const MPNowPlayingInfoPropertyCurrentPlaybackDate = @"MPNowPlayingInfoPropertyCurrentPlaybackDate";

// Instance variables sit in the interfaces because this is also built for i386 (old runtime).
@interface MPNowPlayingInfoCenter : NSObject {
    NSDictionary *_nowPlayingInfo;
    NSUInteger _playbackState;
}
+ (MPNowPlayingInfoCenter *)defaultCenter;
@property (copy) NSDictionary *nowPlayingInfo;
@property NSUInteger playbackState;   // MPNowPlayingPlaybackState; 0 is "unknown"
@end

@implementation MPNowPlayingInfoCenter
@synthesize nowPlayingInfo = _nowPlayingInfo;
@synthesize playbackState = _playbackState;

+ (MPNowPlayingInfoCenter *)defaultCenter
{
    static MPNowPlayingInfoCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        center = [[MPNowPlayingInfoCenter alloc] init];
    });
    return center;
}
@end

@interface MPMediaItemArtwork : NSObject {
    CGSize _boundsSize;
    id (^_requestHandler)(CGSize);
}
- (instancetype)initWithBoundsSize:(CGSize)boundsSize requestHandler:(id (^)(CGSize size))requestHandler;
- (id)imageWithSize:(CGSize)size;
@property (readonly) CGRect bounds;
@end

@implementation MPMediaItemArtwork

- (instancetype)initWithBoundsSize:(CGSize)boundsSize requestHandler:(id (^)(CGSize size))requestHandler
{
    if ((self = [super init])) {
        _boundsSize = boundsSize;
        _requestHandler = [requestHandler copy];
    }
    return self;
}

- (void)dealloc
{
    [_requestHandler release];
    [super dealloc];
}

- (id)imageWithSize:(CGSize)size
{
    return _requestHandler ? _requestHandler(size) : nil;
}

- (CGRect)bounds
{
    return CGRectMake(0, 0, _boundsSize.width, _boundsSize.height);
}
@end

@interface MPRemoteCommand : NSObject {
    BOOL _enabled;
    NSMutableArray *_targets;   // what was added, so removal has something to remove; never called
}
@property (getter=isEnabled) BOOL enabled;
- (void)addTarget:(id)target action:(SEL)action;
- (id)addTargetWithHandler:(NSInteger (^)(id event))handler;
- (void)removeTarget:(id)target;
- (void)removeTarget:(id)target action:(SEL)action;
@end

@implementation MPRemoteCommand
@synthesize enabled = _enabled;

- (instancetype)init
{
    if ((self = [super init])) {
        _enabled = YES;
        _targets = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [_targets release];
    [super dealloc];
}

// Targets are not retained, as on macOS.
- (void)addTarget:(id)target action:(SEL)action
{
    if (target != nil)
        [_targets addObject:[NSValue valueWithNonretainedObject:target]];
}

- (id)addTargetWithHandler:(NSInteger (^)(id event))handler
{
    id token = [[handler copy] autorelease];
    if (token != nil)
        [_targets addObject:token];
    return token;
}

- (void)removeTarget:(id)target
{
    if (target == nil) {
        [_targets removeAllObjects];
        return;
    }
    [_targets removeObject:target];
    [_targets removeObject:[NSValue valueWithNonretainedObject:target]];
}

- (void)removeTarget:(id)target action:(SEL)action
{
    [self removeTarget:target];
}
@end

#define COMMANDS(X) \
    X(pauseCommand) X(playCommand) X(stopCommand) X(togglePlayPauseCommand) \
    X(enableLanguageOptionCommand) X(disableLanguageOptionCommand) \
    X(changePlaybackRateCommand) X(changeRepeatModeCommand) X(changeShuffleModeCommand) \
    X(nextTrackCommand) X(previousTrackCommand) X(skipForwardCommand) X(skipBackwardCommand) \
    X(seekForwardCommand) X(seekBackwardCommand) X(changePlaybackPositionCommand) \
    X(ratingCommand) X(likeCommand) X(dislikeCommand) X(bookmarkCommand)

@interface MPRemoteCommandCenter : NSObject {
    NSMutableDictionary *_commands;
}
+ (MPRemoteCommandCenter *)sharedCommandCenter;
#define X(name) @property (readonly) MPRemoteCommand *name;
COMMANDS(X)
#undef X
@end

@implementation MPRemoteCommandCenter

+ (MPRemoteCommandCenter *)sharedCommandCenter
{
    static MPRemoteCommandCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        center = [[MPRemoteCommandCenter alloc] init];
    });
    return center;
}

- (instancetype)init
{
    if ((self = [super init])) {
        _commands = [[NSMutableDictionary alloc] init];
#define X(name) _commands[@#name] = [[[MPRemoteCommand alloc] init] autorelease];
        COMMANDS(X)
#undef X
    }
    return self;
}

#define X(name) - (MPRemoteCommand *)name { return _commands[@#name]; }
COMMANDS(X)
#undef X
@end
