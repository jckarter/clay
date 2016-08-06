-- (C) http://www.rsdn.ru/article/funcprog/monad.xml

data Dist a = Dist {
    support :: [a],
    expect :: (a -> Float) -> Float
}

instance Monad Dist where
  return a = Dist 
    {
      support = [a], 
      expect = \f -> f a
    }

  da >>= fdb = Dist 
    {
      support = concat [support (fdb a) | a <- support da],
      expect = \f -> expect da (\a -> expect (fdb a) f)
    }

choose p d1 d2 = Dist {support = s, expect = e}
    where
      s = support d1 ++ support d2
      e f = p * expect d1 f + (1-p) * expect d2 f

prob p = choose p (return True) (return False)

freqs [] = error "Empty cases list"
freqs [(_,a)] = return a
freqs ((w,a):as) = choose w (return a) (freqs as)

mean d = expect d id
disp d = expect d (\x -> (x-m)^2) where m = mean d
probability f d = expect d (\x -> if f x then 1 else 0)

data Light = Red | Green | Yellow
data Driver = Cautious | Normal | Aggressive

data Action = Drive | DontDrive

drive p = choose p (return Drive) (return DontDrive)

_          `actOn` Green  = drive 1.0
Cautious   `actOn` Yellow = drive 0.1
Normal     `actOn` Yellow = drive 0.2
Aggressive `actOn` Yellow = drive 0.9
Cautious   `actOn` Red    = drive 0.0
Normal     `actOn` Red    = drive 0.1
Aggressive `actOn` Red    = drive 0.3

Drive `collision` Drive = prob 0.3
_     `collision` _     = prob 0.0

driver = freqs [(0.2, Cautious), (0.6, Normal), (0.2, Aggressive)]

simulate d1 d2 light = do
  a1 <- d1 `actOn` light
  a2 <- d2 `actOn` light
  a1 `collision` a2

simulateOverDrivers light = do
  d1 <- driver
  d2 <- driver
  simulate d1 d2 light

probCollisionOnRed = probability (==True) (simulateOverDrivers Red)
probCollisionOfTwoAggressiveOnYellow = 
    probability (==True) (simulate Aggressive Aggressive Yellow)

main = do print $ probCollisionOnRed
          print $ probCollisionOfTwoAggressiveOnYellow