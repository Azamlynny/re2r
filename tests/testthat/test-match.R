context("test match")

test_that("test match",{
    tt = function(self, group, res){
        expect_equal(`$.re2_matrix`(self, group), res)
    }
    months <- matrix(
        month.abb[c(12, 1:11)],
        nrow = 3,
        dimnames = list(
            c("start", "middle", "end"),
            c("Winter", "Spring", "Summer", "Fall")
        )
    )
    res <- month.abb[c(12, 1:2)]
    names(res) = c("start", "middle", "end")
    tt(months, 1, res)
    res = month.abb[c(3:5)]
    names(res) = c("start", "middle", "end")
    tt(months, 2, res)
    res = month.abb[c(9:11)]
    names(res) = c("start", "middle", "end")
    tt(months, 4, res)
})
